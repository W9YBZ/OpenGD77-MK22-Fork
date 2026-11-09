/* Actual TX helper functions; fake registers and melody sink, no radio access. */
#include <assert.h>
#include <stdio.h>
#include "../../firmware/source/user_interface/uiTxScreen.c"
volatile int isI2cInUse;
volatile bool trxTransmissionEnabled, txPAEnabled;
settingsStruct_t nonVolatileSettings;
CodeplugChannel_t channel;
CodeplugChannel_t *currentChannelData=&channel;
volatile int16_t *melody_play;
const int16_t MELODY_FM_TALK_PERMIT[]={940,40,0,10,940,40,0,10,940,40,-1,-1};
static uint16_t voiceReg,txReg;
static bool optionOff,noBeep,voiceBusy,voxEnabled,failRead,failWrite,rejectMelody;
static int radioMode,played,writes,rxOff,depth;
static aprsBeaconingMode_t aprsMode;
void vPortEnterCritical(void) {depth++;}
void vPortExitCritical(void) {assert(depth>0);depth--;}
int trxGetMode(void) {return radioMode;}
bool voxIsEnabled(void) {return voxEnabled;}
aprsBeaconingMode_t aprsBeaconingGetMode(void) {return aprsMode;}
bool settingsIsOptionBitSet(bitfieldOptions_t bit) {assert(bit==BIT_FM_TALK_PERMIT_OFF);return optionOff;}
uint8_t codeplugChannelGetFlag(CodeplugChannel_t *ch,ChannelFlag_t flag)
{assert(ch==&channel && flag==CHANNEL_FLAG_NO_BEEP);return noBeep;}
bool voicePromptsIsPlaying(void) {return voiceBusy;}
void soundSetMelody(const int16_t *melody) {if(!rejectMelody){melody_play=(int16_t *)melody;played++;}}
void soundStopMelody(void) {melody_play=NULL;}
void trxActivateRx(bool critical) {assert(critical);txPAEnabled=false;rxOff++;}
status_t radioReadReg2byte(uint8_t reg,uint8_t *hi,uint8_t *lo)
{
 assert(depth && (reg==0x30 || reg==0x3a));if(failRead)return kStatus_Fail;
 uint16_t val=reg==0x30?txReg:voiceReg;*hi=val>>8;*lo=val;return kStatus_Success;
}
status_t radioWriteReg2byte(uint8_t reg,uint8_t hi,uint8_t lo)
{
 assert(depth && reg==0x3a);writes++;if(failWrite)return kStatus_Fail;
 voiceReg=((uint16_t)hi<<8)|lo;return kStatus_Success;
}
static uiEvent_t reset(void)
{
 assert(!depth);talkPermitActive=talkPermitDraining=talkPermitPending=transmitError=false;
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
 assert(!depth);
 puts("PASS: synthetic menu entry / real PTT event / one-shot / key preemption; actual Talk Permit MIC gate/restore/drain/deadline, release/TOT/exit, exclusions, I2C failure and PA shutdown");
}
