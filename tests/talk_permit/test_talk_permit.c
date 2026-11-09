/* Actual TX helper functions; fake registers and melody sink, no radio access. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../firmware/source/user_interface/uiTxScreen.c"
volatile int isI2cInUse;
volatile bool trxTransmissionEnabled, txPAEnabled;
settingsStruct_t nonVolatileSettings;
CodeplugChannel_t channel;
CodeplugChannel_t *currentChannelData=&channel;
volatile int16_t *melody_play;
const int16_t MELODY_FM_TALK_PERMIT[]={940,40,0,10,940,40,0,10,940,40,-1,-1};
static uint16_t voiceReg,txReg,toneReg,gainReg,singleReg;
int trxCurrentBand[2];
static uint32_t now;
static bool calibrationFails,cacheEnabled,ignoreWrite,nonEchoTone;
static unsigned toneReads;
static unsigned reads,failReadNth,failWriteNth;
static uint16_t cache[128];
static bool cached[128];
uint32_t ticksGetMillis(void) {return now;}
bool calibrationGetSectionData(CalibrationBand_t band, CalibrationSection_t section, CalibrationDataResult_t *o)
{
 assert(section==CalibrationSection_DEV_TONE && o->offset==1);
 assert(band==(trxCurrentBand[TRX_TX_FREQ_BAND]==RADIO_BAND_UHF ? CalibrationBandUHF : CalibrationBandVHF));
 o->value=0x2a;return !calibrationFails;
}
static uint16_t *regPtr(uint8_t reg)
{
 switch(reg){case 0x30:return &txReg;case 0x3a:return &voiceReg;
 case 0x35:return &toneReg;case 0x41:return &gainReg;case 0x79:return &singleReg;}
 assert(0);return NULL;
}
static bool optionOff,noBeep,voiceBusy,voxEnabled,failRead,failWrite,rejectMelody;
static int radioMode,played,writes,rxOff,depth;
static aprsBeaconingMode_t aprsMode;
void vPortEnterCritical(void) {depth++;}
void vPortExitCritical(void) {assert(depth>0);depth--;}
int trxGetMode(void) {return radioMode;}
bool voxIsEnabled(void) {return voxEnabled;}
aprsBeaconingMode_t aprsBeaconingGetMode(void) {return aprsMode;}
bool settingsIsOptionBitSet(bitfieldOptions_t bit) {
 if(bit==BIT_FM_TALK_PERMIT_OFF)return optionOff;
 assert(bit==BIT_FM_PTT_TONE_LOW || bit==BIT_FM_PTT_TONE_HIGH);
 return (nonVolatileSettings.bitfieldOptions[0] & bit) != 0;
}
uint8_t codeplugChannelGetFlag(CodeplugChannel_t *ch,ChannelFlag_t flag)
{assert(ch==&channel && flag==CHANNEL_FLAG_NO_BEEP);return noBeep;}
bool voicePromptsIsPlaying(void) {return voiceBusy;}
void soundSetMelody(const int16_t *melody) {if(!rejectMelody){melody_play=(int16_t *)melody;played++;}}
void soundStopMelody(void) {melody_play=NULL;}
void trxActivateRx(bool critical) {assert(critical);txPAEnabled=false;rxOff++;}
status_t radioReadReg2byte(uint8_t reg,uint8_t *hi,uint8_t *lo)
{
 assert(depth);reads++;if(failRead || reads==failReadNth)return kStatus_Fail;
 uint16_t val=*regPtr(reg);
 if(reg==0x35){toneReads++;if(nonEchoTone)val^=0x4000;}
 *hi=val>>8;*lo=val;return kStatus_Success;
}
status_t radioWriteReg2byte(uint8_t reg,uint8_t hi,uint8_t lo)
{
 assert(depth);writes++;
 uint16_t val=((uint16_t)hi<<8)|lo;
 if(cacheEnabled && cached[reg] && cache[reg]==val)return kStatus_Success;
 cache[reg]=val;cached[reg]=true; // Model the native driver's cache-on-failure.
 if(failWrite || (unsigned)writes==failWriteNth)return kStatus_Fail;
 if(!ignoreWrite)*regPtr(reg)=val;
 return kStatus_Success;
}
status_t radioWriteReg2byteFresh(uint8_t reg,uint8_t hi,uint8_t lo)
{
 assert(depth);
 if(isI2cInUse)return kStatus_Fail;
 cached[reg]=false;
 status_t status=radioWriteReg2byte(reg,hi,lo);
 if(status!=kStatus_Success)cached[reg]=false;
 return status;
}
static uiEvent_t reset(void)
{
 assert(!depth);talkPermitActive=talkPermitDraining=talkPermitPending=transmitError=false;
 pttToneActive=pttToneArmed=false;fmAudioFault=0;now=100;reads=failReadNth=failWriteNth=0;
 calibrationFails=cacheEnabled=ignoreWrite=nonEchoTone=false;toneReads=0;memset(cached,0,sizeof(cached));
 nonVolatileSettings.bitfieldOptions[0]=0;
 toneReg=0x4567;gainReg=0x6f35;singleReg=0x0211;
 trxCurrentBand[TRX_TX_FREQ_BAND]=RADIO_BAND_UHF;
 isSatelliteScreen=optionOff=noBeep=voiceBusy=voxEnabled=failRead=failWrite=rejectMelody=false;
 isI2cInUse=0;voiceReg=0x40ac;txReg=0x7046;radioMode=RADIO_MODE_ANALOG;
 trxTransmissionEnabled=txPAEnabled=true;melody_play=NULL;played=writes=rxOff=0;
 channel.tot=0;timeInSeconds=5;aprsMode=APRS_BEACONING_MODE_OFF;
 nonVolatileSettings.audioPromptMode=AUDIO_PROMPT_MODE_BEEP;
 uiEvent_t event={0};event.time=100;event.buttons=BUTTON_PTT;return event;
}
int main(void)
{
 // Reproduce the real menu sequence: synthetic first-run has no PTT.
 uiEvent_t ev=reset();ev.buttons=0;talkPermitStart(&ev);
 assert(!talkPermitActive && !played && !writes);
 talkPermitPending=true;ev.buttons=BUTTON_PTT;ev.hasEvent=true;
 talkPermitTick(&ev);
 assert(talkPermitActive && played==1 && !talkPermitPending);
 ev.time++;ev.hasEvent=false;talkPermitTick(&ev);assert(played==1);
 ev.keys.key=KEY_1;ev.hasEvent=true;talkPermitTick(&ev);
 assert(!talkPermitActive && voiceReg==0x40ac);
 ev=reset();talkPermitPending=true;ev.hasEvent=true;talkPermitTick(&ev);
 ev.buttons|=BUTTON_SK2;talkPermitTick(&ev);
 assert(!talkPermitActive && voiceReg==0x40ac);
 ev=reset();talkPermitPending=true;ev.buttons=0;talkPermitTick(&ev);
 assert(!talkPermitPending && !played && !writes);
 ev.buttons=BUTTON_PTT;talkPermitTick(&ev);assert(!played);
 ev=reset();talkPermitPending=true;talkPermitExit(NULL);talkPermitTick(&ev);assert(!played);
 ev=reset();talkPermitStart(&ev);
 assert(talkPermitActive && played==1 && voiceReg==0x00ac && talkPermitVoice==0x40ac);
 ev.time=240;melody_play=NULL;talkPermitTick(&ev);assert(talkPermitActive && voiceReg==0x00ac);
 ev.time=249;talkPermitTick(&ev);assert(talkPermitActive);
 ev.time=250;talkPermitTick(&ev);assert(!talkPermitActive && voiceReg==0x40ac && !rxOff);
 ev=reset();talkPermitStart(&ev);ev.time=350;talkPermitTick(&ev);
 assert(!talkPermitActive && !melody_play && voiceReg==0x40ac);
 ev=reset();talkPermitStart(&ev);ev.buttons=0;talkPermitTick(&ev);
 assert(!talkPermitActive && voiceReg==0x40ac);
 ev=reset();talkPermitStart(&ev);talkPermitExit(NULL);assert(!talkPermitActive && voiceReg==0x40ac);
 ev=reset();talkPermitStart(&ev);transmitError=true;talkPermitTick(&ev);assert(!talkPermitActive);
 ev=reset();channel.tot=1;talkPermitStart(&ev);timeInSeconds=0;talkPermitTick(&ev);assert(!talkPermitActive);
 // Do not take a MIC snapshot or touch registers for excluded sessions.
 for(int n=0;n<13;n++)
 {
  ev=reset();switch(n){case 0:optionOff=true;break;case 1:noBeep=true;break;
  case 2:voiceBusy=true;break;case 3:voxEnabled=true;break;case 4:isSatelliteScreen=true;break;
  case 5:aprsMode=APRS_BEACONING_MODE_PTT;break;case 6:radioMode=RADIO_MODE_DIGITAL;break;
  case 7:trxTransmissionEnabled=false;break;case 8:txPAEnabled=false;break;case 9:ev.buttons=0;break;
  case 10:nonVolatileSettings.audioPromptMode=AUDIO_PROMPT_MODE_SILENT;break;
  case 11:isI2cInUse=4;break;default:txReg=0x7026;break;}
  talkPermitStart(&ev);assert(!talkPermitActive && !played && !writes);
 }
 ev=reset();melody_play=(int16_t *)1;talkPermitStart(&ev);assert(!played && !writes);
 ev=reset();failRead=true;talkPermitStart(&ev);assert(!writes && !talkPermitActive);
 ev=reset();rejectMelody=true;talkPermitStart(&ev);assert(!talkPermitActive && voiceReg==0x40ac);
 ev=reset();talkPermitStart(&ev);failWrite=true;talkPermitExit(NULL);
 assert(talkPermitActive && !txPAEnabled && !trxTransmissionEnabled && transmitError && rxOff==1);
 failWrite=false;talkPermitExit(NULL);assert(!talkPermitActive && voiceReg==0x40ac);
 ev=reset();failWrite=true;talkPermitStart(&ev);assert(!played && !txPAEnabled && transmitError);
 failWrite=false;talkPermitExit(NULL);
 // Monotonic time may wrap during an active melody.
 ev=reset();ev.time=0xfffffff0;talkPermitStart(&ev);ev.time+=250;talkPermitTick(&ev);assert(!talkPermitActive);
 // PTT RF tail: arm from the same real-event path even with Talk Permit Off.
 ev=reset();optionOff=true;nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 talkPermitPending=true;ev.hasEvent=true;talkPermitTick(&ev);
 assert(pttToneArmed && !talkPermitActive && !played);
 ev.buttons=0;assert(pttToneHandleRelease(&ev));
 assert(pttToneActive && !pttToneArmed && voiceReg==0x10ac && toneReg==10000);
 assert(gainReg==0x6f2a && singleReg==0xc211 && !played);
 ev.time=249;assert(pttToneHandleRelease(&ev));assert(txPAEnabled);
 ev.time=250;assert(!pttToneHandleRelease(&ev));
 assert(!pttToneActive && voiceReg==0x40ac && toneReg==0 && gainReg==0x6f35 && singleReg==0x0211);
 int doneWrites=writes;assert(!pttToneHandleRelease(&ev));assert(writes==doneWrites);
 // RF tail is independent of local Silent/no-beep policy and uses TX-band calibration.
 ev=reset();optionOff=true;noBeep=true;nonVolatileSettings.audioPromptMode=AUDIO_PROMPT_MODE_SILENT;
 trxCurrentBand[TRX_TX_FREQ_BAND]=RADIO_BAND_VHF;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;pttToneArmed=true;
 ev.buttons=0;assert(pttToneHandleRelease(&ev));pttToneExit();assert(!pttToneActive);
 // Exclusions, reserved MDC and invalid mode never emit an RF tail.
 for(int n=0;n<13;n++)
 {
  ev=reset();ev.buttons=0;pttToneArmed=true;
  nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
  switch(n){case 0:nonVolatileSettings.bitfieldOptions[0]=0;break;
  case 1:nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_HIGH;break;
  case 2:nonVolatileSettings.bitfieldOptions[0]|=BIT_FM_PTT_TONE_HIGH;break;
  case 3:radioMode=RADIO_MODE_DIGITAL;break;case 4:voxEnabled=true;break;
  case 5:isSatelliteScreen=true;break;case 6:aprsMode=APRS_BEACONING_MODE_PTT;break;
  case 7:trxTransmissionEnabled=false;break;case 8:txPAEnabled=false;break;
  case 9:transmitError=true;break;case 10:channel.tot=1;timeInSeconds=0;break;
  case 11:ev.buttons=BUTTON_PTT;break;case 12:pttToneArmed=false;break;}
  assert(!pttToneHandleRelease(&ev));assert(!pttToneActive && !writes);
 }
 // Lost hardware TX, calibration failure, I2C contention and snapshot failure.
 for(int n=0;n<7;n++)
 {
  ev=reset();ev.buttons=0;pttToneArmed=true;nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
  switch(n){case 0:txReg=0x7026;break;case 1:calibrationFails=true;break;
  case 2:isI2cInUse=4;break;default:failReadNth=n-2;break;}
  assert(!pttToneHandleRelease(&ev));assert(!pttToneActive && !txPAEnabled && transmitError && !writes);
 }
 // Fail each setup write or its readback, and restore every partially changed register.
 for(int n=1;n<=7;n++)
 {
  ev=reset();ev.buttons=0;pttToneArmed=true;cacheEnabled=true;
  nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
  if(n<=4)failWriteNth=n;else failReadNth=n;
  assert(!pttToneHandleRelease(&ev));
  assert(!txPAEnabled && !trxTransmissionEnabled && transmitError);
  assert(!pttToneActive && voiceReg==0x40ac && toneReg==0 && gainReg==0x6f35 && singleReg==0x0211);
 }
 // Reproduce E35/4's unvalidated echo assumption: a non-echoing frequency
 // register must not be read/snapshotted or prevent tail setup and future TX.
 ev=reset();nonEchoTone=true;pttToneArmed=true;ev.buttons=0;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 assert(pttToneHandleRelease(&ev));assert(toneReg==10000 && toneReads==0);
 ev.time=250;assert(!pttToneHandleRelease(&ev));
 assert(!pttToneActive && toneReg==0 && voiceReg==0x40ac && toneReads==0 && !fmAudioFault);
 // A physical frequency-command failure still aborts; it is never ignored.
 ev=reset();pttToneArmed=true;ev.buttons=0;failWriteNth=1;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 assert(!pttToneHandleRelease(&ev));
 assert(!txPAEnabled && transmitError && !pttToneActive && fmAudioFault==0x0235);
 // An idle-command fault cannot retain ownership after verified MIC restore.
 ev=reset();pttToneArmed=true;ev.buttons=0;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 assert(pttToneHandleRelease(&ev));failWriteNth=writes+4;
 fmTxAudioExit(NULL);assert(!pttToneActive && !txPAEnabled && transmitError && voiceReg==0x40ac);
 assert(fmAudioFault==0x0235);
 transmitError=false;fmTxAudioExit(NULL);assert(!transmitError && !pttToneActive);
 puts("PASS: 0x35 non-echo command model, no frequency reads/snapshots, actual write failures abort, idle failure cannot latch verified MIC");
 // Live tail aborts on TOT, TX failure or exit; wraparound remains bounded.
 for(int n=0;n<4;n++)
 {
  ev=reset();ev.buttons=0;pttToneArmed=true;nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
  now=0xfffffff0;ev.time=now;assert(pttToneHandleRelease(&ev));
  switch(n){case 0:channel.tot=1;timeInSeconds=0;break;case 1:transmitError=true;break;
  case 2:txPAEnabled=false;break;case 3:ev.time+=150;break;}
  assert(!pttToneHandleRelease(&ev));assert(!pttToneActive && voiceReg==0x40ac);
 }
 // Rapid PTT re-press cannot extend the original tail deadline or start a second tail.
 ev=reset();ev.buttons=0;pttToneArmed=true;nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 assert(pttToneHandleRelease(&ev));ev.buttons=BUTTON_PTT;ev.time=249;
 assert(pttToneHandleRelease(&ev));ev.time=250;assert(!pttToneHandleRelease(&ev));
 assert(!pttToneHandleRelease(&ev) && !pttToneActive);
 // A transient restore failure is retried physically, despite a warm cache.
 ev=reset();ev.buttons=0;pttToneArmed=true;cacheEnabled=true;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 assert(pttToneHandleRelease(&ev));failWriteNth=writes+1;
 fmTxAudioExit(NULL);assert(!pttToneActive && txPAEnabled && !transmitError);
 assert(menuTxScreenGetAudioFault()==0x023a && voiceReg==0x40ac);
 // Persistent faults still disable PA and retain ownership; recovery can retry.
 ev=reset();ev.buttons=0;pttToneArmed=true;cacheEnabled=true;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 assert(pttToneHandleRelease(&ev));failWrite=true;
 fmTxAudioExit(NULL);assert(pttToneActive && !txPAEnabled && transmitError);
 failWrite=false;fmTxAudioExit(NULL);assert(!pttToneActive && voiceReg==0x40ac);
 // ACKed writes with a wrong physical readback must remain fail-closed.
 ev=reset();ev.buttons=0;pttToneArmed=true;
 nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 assert(pttToneHandleRelease(&ev));ignoreWrite=true;fmTxAudioExit(NULL);
 assert(pttToneActive && !txPAEnabled && transmitError);
 assert(menuTxScreenGetAudioFault()==0x0479);
 ignoreWrite=false;fmTxAudioExit(NULL);assert(!pttToneActive && voiceReg==0x40ac);
 // Consecutive sessions MUST keep real register/cache state across each TX.
 // Native RX changes the CSS bits in 3a between sessions; next TX preserves them.
 for(int option=0;option<2;option++)
 {
  ev=reset();cacheEnabled=true;optionOff=true;nonEchoTone=true;
  nonVolatileSettings.bitfieldOptions[0]=option?BIT_FM_PTT_TONE_LOW:0;
  for(int session=0;session<100;session++)
  {
   transmitError=false;fmTxAudioExit(NULL);assert(!transmitError && !pttToneActive);
   trxTransmissionEnabled=txPAEnabled=true;txReg=0x7046;
   voiceReg=0x40ac;pttWasReleased=false;
   now+=500;ev.time=now;ev.buttons=BUTTON_PTT;ev.hasEvent=true;
   talkPermitPending=true;talkPermitTick(&ev);assert(pttToneArmed);
   ev.buttons=0;talkPermitTick(&ev);
   bool wait=pttToneHandleRelease(&ev);assert(wait==(option!=0));
   if(wait){now+=150;ev.time=now;assert(!pttToneHandleRelease(&ev));}
   trxTransmissionEnabled=false;trxActivateRx(true);txReg=0x7026;
   // Simulate native CSS RX setup before menu-pop's callback.
   voiceReg=(voiceReg&0xffe0)|1;
   fmTxAudioExit(NULL);assert(!pttToneActive && !talkPermitActive && !transmitError);
  }
  assert(toneReads==0);
 }
 puts("PASS: 100 consecutive Off and 100 consecutive Beep sessions retain hardware/cache state through native RX/menu cleanup");
 // Early release during Talk Permit restores MIC before the tail snapshots it.
 ev=reset();talkPermitPending=true;nonVolatileSettings.bitfieldOptions[0]=BIT_FM_PTT_TONE_LOW;
 talkPermitTick(&ev);assert(talkPermitActive && pttToneArmed);
 ev.buttons=0;talkPermitTick(&ev);assert(!talkPermitActive && voiceReg==0x40ac);
 assert(pttToneHandleRelease(&ev));fmTxAudioExit(NULL);assert(!pttToneActive && voiceReg==0x40ac);
 puts("PASS: PTT RF tail 1000 Hz / 150 ms state, one-shot, mode exclusions, route/gain restore and tone idle, TX-band calibration, setup/readback/cache failures, TOT/exit/re-key/wrap and Talk Permit coexistence");
 assert(!depth);
 puts("PASS: synthetic menu entry / real PTT event / one-shot / key preemption; actual Talk Permit MIC gate/restore/drain/deadline, release/TOT/exit, exclusions, I2C failure and PA shutdown");
}
