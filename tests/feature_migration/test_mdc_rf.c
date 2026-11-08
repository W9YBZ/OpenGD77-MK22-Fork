/* Actual board ISR code, simulated MK22 registers, no radio/device access. */
#define GD77_EXPERIMENTAL_MDC_RF 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
static PIT_Type mockPIT;
static I2C_Type mockI2C;
#undef PIT
#undef I2C0
#define PIT (&mockPIT)
#define I2C0 (&mockI2C)
static unsigned priorities[128],enabled[128];
#undef NVIC_GetEnableIRQ
#undef NVIC_GetPriority
#undef NVIC_SetPriority
#undef NVIC_ClearPendingIRQ
#undef EnableIRQ
#undef DisableIRQ
#undef __DSB
#define NVIC_GetEnableIRQ(n) (enabled[n])
#define NVIC_GetPriority(n) (priorities[n])
#define NVIC_SetPriority(n,p) (priorities[n]=(p))
#define NVIC_ClearPendingIRQ(n) ((void)(n))
#define EnableIRQ(n) (enabled[n]=1)
#define DisableIRQ(n) (enabled[n]=0)
#define __DSB() ((void)0)
#undef GPIO_PinWrite
#define GPIO_PinWrite(gpio,pin,value) ((void)(gpio),(void)(pin),assert((value)==0))
#include "../../firmware/source/functions/mdc1200RF.c"
volatile int isI2cInUse;
volatile bool trxTransmissionEnabled,txPAEnabled;
static uint32_t clockHz=61440000;
static int invalidations,mode=RADIO_MODE_ANALOG;
void vPortEnterCritical(void) {}
void vPortExitCritical(void) {}
uint32_t CLOCK_GetFreq(clock_name_t clock) {(void)clock;return clockHz;}
int trxGetMode(void) {return mode;}
bool aprsBeaconingIsTransmitting(void) {return false;}
void radioInvalidateRegisterCache(uint8_t reg) {assert(reg==0x35);invalidations++;}
static void reset(void)
{
 mdcRFStop();
 memset(&mockPIT,0,sizeof(mockPIT));memset(&mockI2C,0,sizeof(mockI2C));
 memset(priorities,0,sizeof(priorities));memset(enabled,0,sizeof(enabled));
 mockPIT.CHANNEL[0].TCTRL=0xa5;mockPIT.CHANNEL[0].LDVAL=0x1234;
 mockPIT.CHANNEL[1].LDVAL=55;mockPIT.CHANNEL[2].LDVAL=66;
 *(volatile uint32_t *)(void *)&mockPIT.CHANNEL[2].CVAL=0xfffffff0;
 mockI2C.C1=I2C_C1_IICEN_MASK;
 isI2cInUse=invalidations=0;trxTransmissionEnabled=txPAEnabled=true;
 clockHz=61440000;mode=RADIO_MODE_ANALOG;
 priorities[I2C0_IRQn]=8;priorities[PIT1_IRQn]=9;
}
static void boundary(void)
{
 *(volatile uint32_t *)(void *)&mockPIT.CHANNEL[2].CVAL=~schedule.deadline;
 PIT1_IRQHandler();
}
static void ackWrite(void)
{
 assert(pending && isI2cInUse==7);
 assert(mockI2C.D==(AT1846S_I2C_MASTER_SLAVE_ADDR_7BIT<<1));
 uint32_t when=writeStarted;
 uint8_t expected[]={0x35,txBytes[1],txBytes[2]};
 for(int n=0;n<4;n++) {
  when+=clockHz/1000000*23;
  *(volatile uint32_t *)(void *)&mockPIT.CHANNEL[2].CVAL=~when;mockI2C.S=I2C_S_IICIF_MASK;
  I2C0_IRQHandler();
  if(n<3)assert(mockI2C.D==expected[n]);
 }
 assert(!pending && !isI2cInUse);
 assert(!(mockI2C.C1&(I2C_C1_MST_MASK|I2C_C1_IICIE_MASK)));
}
int main(void)
{
 const unsigned ids[]={1,0x1234,0xffff};
 for(unsigned id=0;id<3;id++) {
  reset();assert(mdcRFStart(ids[id]));
  uint8_t unchanged[26];memcpy(unchanged,tones,26);
  assert(!mdcRFStart(0x4321) && !memcmp(unchanged,tones,26)); // no reentrant corruption
  for(unsigned n=1;n<=208;n++) {
   boundary();if(pending)ackWrite();
   assert(mdcRFStatus()==(n==208 ? MDC_RF_DONE : MDC_RF_RUNNING));
  }
  mdcRFDiagnostics_t d=mdcRFDiagnostics();
  assert(d.unitId==ids[id] && d.symbols==208 && d.reason==0 && d.writes>0);
  unsigned writes=d.writes;PIT1_IRQHandler();assert(diag.writes==writes);
  mdcRFStop();
  assert(!enabled[PIT1_IRQn]&&!enabled[I2C0_IRQn]&&priorities[I2C0_IRQn]==8&&priorities[PIT1_IRQn]==9);
  assert(mockPIT.CHANNEL[0].TCTRL==0xa5&&mockPIT.CHANNEL[0].LDVAL==0x1234);
  assert(mockPIT.CHANNEL[1].LDVAL==55&&mockPIT.CHANNEL[2].LDVAL==66);
  assert(invalidations==2);
 }
 reset();assert(!mdcRFStart(0));
 reset();isI2cInUse=2;assert(!mdcRFStart(1));
 reset();enabled[I2C0_IRQn]=1;assert(!mdcRFStart(1));
 reset();mockPIT.CHANNEL[2].TCTRL=PIT_TCTRL_TEN_MASK;assert(!mdcRFStart(1));
 reset();mode=RADIO_MODE_DIGITAL;assert(!mdcRFStart(1));
 reset();txPAEnabled=false;assert(!mdcRFStart(1));
 reset();assert(mdcRFStart(1));clockHz=48000000;assert(mdcRFStatus()==MDC_RF_FAULT);
 reset();assert(mdcRFStart(1));trxTransmissionEnabled=false;boundary();assert(status==MDC_RF_FAULT);
 reset();assert(mdcRFStart(1));*(volatile uint32_t *)(void *)&mockPIT.CHANNEL[2].CVAL=~(schedule.deadline+schedule.lateLimit+1);
 PIT1_IRQHandler();assert(status==MDC_RF_FAULT);
 // First transition is in sync, after the steady 1800-Hz leader.
 reset();assert(mdcRFStart(1));while(!pending)boundary();
 mockI2C.S=I2C_S_IICIF_MASK|I2C_S_RXAK_MASK;I2C0_IRQHandler();
 assert(status==MDC_RF_FAULT && !pending && isI2cInUse==0);
 reset();assert(mdcRFStart(1));while(!pending)boundary();
 boundary();assert(status==MDC_RF_FAULT && !pending && isI2cInUse==0);
 reset();assert(mdcRFStart(1));while(schedule.symbol<56)boundary();
 isI2cInUse=2;boundary(); // background I2C contention at first required change
 assert(status==MDC_RF_FAULT && isI2cInUse==2);
 reset();assert(mdcRFStart(1));while(!pending)boundary();mdcRFStop();
 assert(!owned && !pending && isI2cInUse==0);
 puts("PASS: actual MDC RF IRQ adapter: 3 IDs/208 symbols, ACK bytes, NACK, contention, deadlines, TX loss, cancel and timer restore");
}
