/* New experimental adapter. Repository license.txt applies to this file.
 * Explicit lab builds link a separate GPL-2.0 codec: distribution compatibility
 * is unresolved. No claim of RF decoding success. No old MDC implementation used.
 */
#include "functions/mdc1200RF.h"
#if GD77_EXPERIMENTAL_MDC_RF
#include "main.h"
#include "functions/mdc1200Schedule.h"
#include "functions/aprs.h"
#include "mdc1200_codec.h"
#if I2C_RETRY_TIMES == 0
#error "Experimental RF requires bounded SDK I2C retries in ALL translation units"
#endif
#define MDC_BUS_OWNER 7
#define MDC_MAX_WRITE_US 200U
static mdcSchedule_t schedule;
static uint8_t tones[26], txBytes[3], byteIndex;
static volatile bool owned, pending;
static volatile mdcRFStatus_t status;
static volatile mdcRFDiagnostics_t diag;
static uint32_t writeStarted, savedI2CPriority, savedPITPriority;
static uint32_t savedLoad1,savedLoad2,savedControl1,savedControl2;

static uint32_t nowTicks(void) {return ~PIT_GetCurrentTimerCount(PIT,kPIT_Chnl_2);}
static void failRF(uint32_t reason)
{
 if(status!=MDC_RF_FAULT || !diag.reason)diag.reason=reason;
 status=MDC_RF_FAULT;
 // Bounded fault response independent of foreground/UI scheduling and I2C.
 // Do not clear trxTransmissionEnabled: foreground still owes TX->RX cleanup.
 GPIO_PinWrite(GPIO_VHF_TX_amp_power,Pin_VHF_TX_amp_power,0);
 GPIO_PinWrite(GPIO_UHF_TX_amp_power,Pin_UHF_TX_amp_power,0);
 txPAEnabled=false;
}
static void finishWrite(bool failed)
{
 // STOP without polling; no driver abort helper (that helper can spin forever).
 I2C0->C1&=~(I2C_C1_MST_MASK|I2C_C1_TX_MASK|I2C_C1_IICIE_MASK);
 uint32_t elapsed=nowTicks()-writeStarted;
 if(elapsed>diag.maxWriteTicks)diag.maxWriteTicks=elapsed;
 if(failed || elapsed>diag.busHz/1000000U*MDC_MAX_WRITE_US) {
  failRF(failed ? 2U : 3U);
 } else diag.writes++;
 pending=false;
 if(isI2cInUse==MDC_BUS_OWNER)isI2cInUse=0;
}
static bool beginWrite(uint16_t frequency10)
{
 if(isI2cInUse || (I2C0->S&I2C_S_BUSY_MASK))return false;
 isI2cInUse=MDC_BUS_OWNER;
 txBytes[0]=0x35;txBytes[1]=frequency10>>8;txBytes[2]=frequency10&255;
 byteIndex=0;writeStarted=nowTicks();pending=true;
 I2C0->S=I2C_S_IICIF_MASK|I2C_S_ARBL_MASK;
 I2C0->C1|=I2C_C1_TX_MASK|I2C_C1_MST_MASK|I2C_C1_IICIE_MASK;
 I2C0->D=AT1846S_I2C_MASTER_SLAVE_ADDR_7BIT<<1;
 return true;
}
void I2C0_IRQHandler(void)
{
 if(!owned || !pending)return;
 uint8_t flags=I2C0->S;
 if(!(flags&I2C_S_IICIF_MASK))return;
 I2C0->S=I2C_S_IICIF_MASK|(flags&I2C_S_ARBL_MASK);
 if(flags&(I2C_S_RXAK_MASK|I2C_S_ARBL_MASK))finishWrite(true);
 else if(byteIndex<3)I2C0->D=txBytes[byteIndex++];
 else finishWrite(false);
 __DSB();
}
static void armDeadline(void)
{
 PIT_StopTimer(PIT,kPIT_Chnl_1);
 int32_t left=(int32_t)(schedule.deadline-nowTicks());
 if(left<=0) {failRF(4);return;}
 PIT_SetTimerPeriod(PIT,kPIT_Chnl_1,(uint32_t)left);
 PIT_StartTimer(PIT,kPIT_Chnl_1);
}
void PIT1_IRQHandler(void)
{
 PIT_ClearStatusFlags(PIT,kPIT_Chnl_1,kPIT_TimerFlag);
 if(!owned || status!=MDC_RF_RUNNING) {
  PIT_StopTimer(PIT,kPIT_Chnl_1);__DSB();return;
 }
 // GPIO PA state and volatile TX flags only; never do synchronous I2C here.
 bool fault=!trxTransmissionEnabled || !txPAEnabled;
 int action=mdcScheduleTick(&schedule,nowTicks(),pending,fault,tones);
 diag.symbols=schedule.symbol;diag.maxLateTicks=schedule.maxLate;
 if(action<0) {failRF(5);}
 else if(schedule.done)status=MDC_RF_DONE;
 else if(action && !beginWrite(schedule.tone ? 18000 : 12000)) {
  failRF(6);
 }
 if(status==MDC_RF_RUNNING)armDeadline();
 else {
  PIT_StopTimer(PIT,kPIT_Chnl_1);
  if(pending)finishWrite(true);
 }
 __DSB();
}
bool mdcRFStart(uint16_t id)
{
 uint8_t frame[26],newTones[26];
 if(mdcEncodePostID(id,frame,sizeof(frame))!=26 || mdcToneBits(frame,26,newTones,26)!=26)return false;
 taskENTER_CRITICAL();
 bool available=!owned && !isI2cInUse && !NVIC_GetEnableIRQ(I2C0_IRQn) &&
  !NVIC_GetEnableIRQ(PIT1_IRQn) && !NVIC_GetEnableIRQ(PIT2_IRQn) && !(I2C0->S&I2C_S_BUSY_MASK) &&
  (I2C0->C1&I2C_C1_IICEN_MASK) && !(I2C0->C1&I2C_C1_IICIE_MASK) &&
  !(PIT->CHANNEL[1].TCTRL&(PIT_TCTRL_TEN_MASK|PIT_TCTRL_TIE_MASK)) &&
  !(PIT->CHANNEL[2].TCTRL&(PIT_TCTRL_TEN_MASK|PIT_TCTRL_TIE_MASK)) &&
  trxTransmissionEnabled && txPAEnabled && trxGetMode()==RADIO_MODE_ANALOG &&
  !aprsBeaconingIsTransmitting();
 if(!available) {taskEXIT_CRITICAL();return false;}
 memcpy(tones,newTones,sizeof(tones));
 savedLoad1=PIT->CHANNEL[1].LDVAL;savedLoad2=PIT->CHANNEL[2].LDVAL;
 savedControl1=PIT->CHANNEL[1].TCTRL;savedControl2=PIT->CHANNEL[2].TCTRL;
 savedI2CPriority=NVIC_GetPriority(I2C0_IRQn);savedPITPriority=NVIC_GetPriority(PIT1_IRQn);
 uint32_t hz=CLOCK_GetFreq(kCLOCK_BusClk);
 diag=(mdcRFDiagnostics_t){0};diag.busHz=hz;diag.unitId=id;
 PIT_SetTimerPeriod(PIT,kPIT_Chnl_2,UINT32_MAX);
 PIT_StartTimer(PIT,kPIT_Chnl_2);
 if(!mdcScheduleInit(&schedule,hz,nowTicks())) {
  PIT->CHANNEL[2].TCTRL=savedControl2;PIT->CHANNEL[2].LDVAL=savedLoad2;
  taskEXIT_CRITICAL();return false;
 }
 owned=true;pending=false;status=MDC_RF_RUNNING;
 radioInvalidateRegisterCache(0x35);
 NVIC_SetPriority(I2C0_IRQn,3);NVIC_SetPriority(PIT1_IRQn,3);
 NVIC_ClearPendingIRQ(I2C0_IRQn);NVIC_ClearPendingIRQ(PIT1_IRQn);
 PIT_ClearStatusFlags(PIT,kPIT_Chnl_1,kPIT_TimerFlag);
 PIT_EnableInterrupts(PIT,kPIT_Chnl_1,kPIT_TimerInterruptEnable);
 EnableIRQ(I2C0_IRQn);EnableIRQ(PIT1_IRQn);
 armDeadline();
 taskEXIT_CRITICAL();return status==MDC_RF_RUNNING;
}
mdcRFStatus_t mdcRFStatus(void)
{
 if(owned && status==MDC_RF_RUNNING && CLOCK_GetFreq(kCLOCK_BusClk)!=diag.busHz) {
  taskENTER_CRITICAL();failRF(7);taskEXIT_CRITICAL();
 }
 return status;
}
void mdcRFStop(void)
{
 taskENTER_CRITICAL();
 if(owned) {
  if(status==MDC_RF_RUNNING)failRF(8); // cancellation, retain first root cause
  PIT_StopTimer(PIT,kPIT_Chnl_1);PIT_StopTimer(PIT,kPIT_Chnl_2);
  DisableIRQ(PIT1_IRQn);DisableIRQ(I2C0_IRQn);
  if(pending)finishWrite(true);
  radioInvalidateRegisterCache(0x35);
  PIT->CHANNEL[1].TCTRL=savedControl1;PIT->CHANNEL[1].LDVAL=savedLoad1;
  PIT->CHANNEL[2].TCTRL=savedControl2;PIT->CHANNEL[2].LDVAL=savedLoad2;
  NVIC_ClearPendingIRQ(I2C0_IRQn);NVIC_ClearPendingIRQ(PIT1_IRQn);
  NVIC_SetPriority(I2C0_IRQn,savedI2CPriority);NVIC_SetPriority(PIT1_IRQn,savedPITPriority);
  owned=false;
 }
 taskEXIT_CRITICAL();
}
mdcRFDiagnostics_t mdcRFDiagnostics(void)
{
 taskENTER_CRITICAL();mdcRFDiagnostics_t copy=diag;taskEXIT_CRITICAL();return copy;
}
#endif
