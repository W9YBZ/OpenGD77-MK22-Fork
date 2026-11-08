/* Exercise actual GD77 helper bodies with fake registers, never radio hardware. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../firmware/source/user_interface/uiTxScreen.c"

volatile int isI2cInUse;
volatile bool trxTransmissionEnabled, trxIsTransmitting, txPAEnabled;
settingsStruct_t nonVolatileSettings;
CodeplugChannel_t channel;
CodeplugChannel_t *currentChannelData = &channel;
int trxCurrentBand[2];
volatile int16_t *melody_play;
const int16_t MELODY_FM_TALK_PERMIT[] = {940,40,0,10,940,40,0,10,940,40,-1,-1};
static uint16_t regs[128], original[128];
static int failWrite, writeCount, failRead, readCount, played;
static bool voiceBusy;
static fmFeatures_t config;
#if GD77_EXPERIMENTAL_MDC_RF
static mdcRFStatus_t fakeRF;
static unsigned rfStarts;
static bool rfStartFails;
bool mdcRFStart(uint16_t id) {assert(id==config.unitId);rfStarts++;fakeRF=MDC_RF_RUNNING;return !rfStartFails;}
mdcRFStatus_t mdcRFStatus(void) {return fakeRF;}
void mdcRFStop(void) {fakeRF=MDC_RF_IDLE;}
#endif
void vPortEnterCritical(void) {}
void vPortExitCritical(void) {}
int trxGetMode(void) { return RADIO_MODE_ANALOG; }
uint32_t ticksGetMillis(void) {return 100;}
fmFeatures_t settingsGetFMFeatures(void) {return config;}
uint8_t codeplugChannelGetFlag(CodeplugChannel_t *data, ChannelFlag_t flag)
{(void)data;(void)flag;return 0;}
bool voicePromptsIsPlaying(void) {return voiceBusy;}
void soundSetMelody(const int16_t *melody) {melody_play=(int16_t *)melody;played++;}
void soundStopMelody(void) {melody_play=NULL;}
bool calibrationGetSectionData(CalibrationBand_t band, CalibrationSection_t section, CalibrationDataResult_t *cal)
{(void)band;(void)section;cal->value=32;return true;}
status_t radioReadReg2byte(uint8_t reg,uint8_t *hi,uint8_t *lo)
{
 if (++readCount==failRead) return kStatus_Fail;
 *hi=regs[reg]>>8;*lo=regs[reg];return kStatus_Success;
}
status_t radioWriteReg2byte(uint8_t reg,uint8_t hi,uint8_t lo)
{
 if (++writeCount==failWrite) return kStatus_Fail;
 regs[reg]=((uint16_t)hi<<8)|lo;return kStatus_Success;
}
static void reset(void)
{
 for(int n=0;n<128;n++)regs[n]=0x8000+n;
 regs[0x30]=0x4046;regs[0x3a]=0x40ac;
 memcpy(original,regs,sizeof(regs));
 fmPermitActive=fmTailHardwareActive=transmitError=isTransmittingTone=isTransmittingDTMF=false;
 fmTail=(fmTail_t){0};manualFMSession=trxTransmissionEnabled=txPAEnabled=true;
#if GD77_EXPERIMENTAL_MDC_RF
 fmMDCActive=fmMDCAttempted=rfStartFails=false;rfStarts=0;fakeRF=MDC_RF_IDLE;
#endif
 config=(fmFeatures_t){0x1234,PTT_TONE_BEEP,true};
 nonVolatileSettings.audioPromptMode=AUDIO_PROMPT_MODE_BEEP;
 writeCount=readCount=failWrite=failRead=played=0;voiceBusy=false;melody_play=NULL;
}
int main(void)
{
 reset();fmPermitStart(0);
 assert(fmPermitActive && played==1 && (regs[0x3a]&0x7000)==0);
 assert(fmPermitStop());assert(!memcmp(regs,original,sizeof(regs)));
 reset();fmPermitStart(0);melody_play=NULL;
 assert(fmPermitTick(140,false) && fmPermitActive);
 assert(fmPermitTick(149,false) && fmPermitActive);
 assert(fmPermitTick(150,false) && !fmPermitActive);
 assert(!memcmp(regs,original,sizeof(regs)));
 reset();fmPermitStart(0);assert(fmPermitTick(250,false) && !fmPermitActive);
 reset();fmPermitStart(0);assert(fmPermitTick(1,true) && !fmPermitActive);
 reset();voiceBusy=true;fmPermitStart(0);assert(!played && !writeCount);
 reset();manualFMSession=false;fmPermitStart(0);assert(!played && !writeCount);
 reset();txPAEnabled=false;fmPermitStart(0);assert(!played && !writeCount);
 reset();isI2cInUse=1;fmPermitStart(0);assert(!played && !writeCount);isI2cInUse=0;
 reset();failWrite=1;fmPermitStart(0);assert(!played);assert(!memcmp(regs,original,sizeof(regs)));
 reset();fmPermitStart(0);failWrite=writeCount+1;assert(fmPermitStop()); // retry cleanup
 assert(!memcmp(regs,original,sizeof(regs)));
 reset();assert(fmTailStart());assert(regs[0x35]==10000 && (regs[0x3a]&0x7000)==0x1000);
 assert(!played && !fmPermitActive);assert(fmTailStop());assert(!memcmp(regs,original,sizeof(regs)));
 for (int n=1;n<=4;n++) {
  reset();failWrite=n;assert(!fmTailStart());
  assert(!fmTailHardwareActive);assert(!memcmp(regs,original,sizeof(regs)));
 }
 // Snapshot read failure must not touch any hardware.
 for(int n=1;n<=5;n++) {
  reset();failRead=n;assert(!fmTailStart());assert(!writeCount);
  assert(!memcmp(regs,original,sizeof(regs)));
 }
 reset();isTransmittingDTMF=true;assert(!fmTailStart() && !writeCount);
 reset();isTransmittingTone=true;assert(!fmTailStart() && !writeCount);
 reset();assert(fmTailStart());failWrite=writeCount+1;
 assert(fmTailStop());assert(!memcmp(regs,original,sizeof(regs)));
#if GD77_EXPERIMENTAL_MDC_RF
 reset();config.mode=PTT_TONE_MDC1200;
 assert(fmMDCHandleRelease(true,false,100));
 assert(rfStarts==1 && fmMDCActive && regs[0x35]==18000);
 assert(fmMDCHandleRelease(true,false,101) && rfStarts==1);
 fakeRF=MDC_RF_DONE;
 assert(!fmMDCHandleRelease(true,false,274) && !fmMDCActive && !transmitError);
 assert(!memcmp(regs,original,sizeof(regs)));
 assert(!fmMDCHandleRelease(true,false,275) && rfStarts==1);
 reset();config.mode=PTT_TONE_MDC1200;
 assert(!fmMDCHandleRelease(false,false,100));
 assert(!fmMDCHandleRelease(true,false,101) && rfStarts==0);
 reset();config.mode=PTT_TONE_MDC1200;
 assert(fmMDCHandleRelease(true,false,100));
 assert(!fmMDCHandleRelease(true,true,110) && transmitError && !fmMDCActive);
 assert(!memcmp(regs,original,sizeof(regs)));
 reset();config.mode=PTT_TONE_MDC1200;
 assert(fmMDCHandleRelease(true,false,100));
 assert(!fmMDCHandleRelease(true,false,350) && transmitError && !fmMDCActive);
 reset();config.mode=PTT_TONE_MDC1200;rfStartFails=true;
 assert(!fmMDCHandleRelease(true,false,100) && transmitError && !fmMDCActive);
 assert(!memcmp(regs,original,sizeof(regs)));
 puts("PASS: actual MDC release handler: one attempt, completion, exclusions, TOT, 250ms deadline and start failure cleanup");
#endif
 puts("PASS: actual TX helpers: local MIC gate, RF route, exclusions, I2C failure rollback/retry");
}
