/* Real melody scheduler/worker, fake C6000, GPIO and RTOS; no radio access. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "main.h"
#include "functions/rxPowerSaving.h"
#include "functions/sound.h"
#include "functions/voicePrompts.h"
static unsigned irqEnabled=1, amp, mux=1;
static void pinWrite(GPIO_Type *port, uint32_t pin, uint8_t value)
{
 if(port==GPIO_audio_amp_enable && pin==Pin_audio_amp_enable) amp=value;
 else {assert(port==GPIO_RX_audio_mux && pin==Pin_RX_audio_mux);mux=value;}
}
#undef NVIC_GetEnableIRQ
#undef NVIC_DisableIRQ
#undef NVIC_EnableIRQ
#define NVIC_GetEnableIRQ(n) ((void)(n),irqEnabled)
#define NVIC_DisableIRQ(n) ((void)(n),irqEnabled=0)
#define NVIC_EnableIRQ(n) ((void)(n),irqEnabled=1)
static uint32_t pinRead(GPIO_Type *port, uint32_t pin)
{
 if(port==GPIO_audio_amp_enable && pin==Pin_audio_amp_enable)return amp;
 assert(port==GPIO_RX_audio_mux && pin==Pin_RX_audio_mux);return mux;
}
#define GPIO_PinRead pinRead
#define GPIO_PinWrite pinWrite
#include "../../firmware/source/functions/sound.c"
settingsStruct_t nonVolatileSettings;
CodeplugChannel_t channel;
CodeplugChannel_t *currentChannelData=&channel;
static bool noBeep,voice,scanning;
int16_t *nextKeyBeepMelody;
bool uiVFOModeIsScanning(void) {return scanning;}
bool uiChannelModeIsScanning(void) {return false;}
bool uiVFOModeSweepScanning(bool includePaused) {(void)includePaused;return false;}
#include "key_beep_handler.inc"
static int depth,failOperation=-1,failStatus=-1,operations[5],delays,loopTicks;
static uint8_t fifoValue,modeValue=2,lastPCM[32];
static BaseType_t createResult=pdPASS;
static TaskFunction_t createdTask;
static jmp_buf taskExit;
volatile uint32_t timer_beeptask;
void vPortEnterCritical(void) {depth++;}
void vPortExitCritical(void) {assert(depth>0);depth--;}
void rxPowerSavingSetState(ecoPhase_t state) {(void)state;}
bool rxPowerSavingIsRxOn(void) {return true;}
void voicePromptsTerminateNoTail(void) {voice=false;}
bool voicePromptsIsPlaying(void) {return voice;}
uint8_t codeplugChannelGetFlag(CodeplugChannel_t *c,ChannelFlag_t flag)
{assert(c==&channel && flag==CHANNEL_FLAG_NO_BEEP);return noBeep;}
int trxGetMode(void) {return RADIO_MODE_ANALOG;}
void HRC6000SetDmrRxGain(int8_t gain) {(void)gain;}
BaseType_t xTaskCreate(TaskFunction_t fn,const char *const name,const configSTACK_DEPTH_TYPE stack,void *arg,UBaseType_t priority,TaskHandle_t *handle)
{
 (void)name;(void)arg;assert(stack*sizeof(StackType_t)==1000 && priority==3);
 createdTask=fn;if(createResult==pdPASS)*handle=(TaskHandle_t)1;return createResult;
}
void vTaskDelay(const TickType_t ticks)
{
 assert(irqEnabled && !depth && ticks==0);delays++;
 if(timer_beeptask)timer_beeptask--; // stand-in PIT0 tick
 if(--loopTicks==0)longjmp(taskExit,1);
}
static int op(int which)
{assert(!irqEnabled);operations[which]++;return which==failOperation?failStatus:kStatus_Success;}
int SPI0ClearPageRegByteWithMask(uint8_t page,uint8_t reg,uint8_t mask,uint8_t val)
{assert(page==4 && reg==6 && mask==0xfd);return op(val?0:3);}
int SPI0ReadPageRegByte(uint8_t page,uint8_t reg,volatile uint8_t *val)
{
 assert(page==4);int which=reg==0x88?1:4;int s=op(which);
 if(s==kStatus_Success)*val=which==1?fifoValue:modeValue;
 return s;
}
int SPI0WritePageRegByteArray(uint8_t page,uint8_t reg,const uint8_t *values,uint8_t count)
{
 assert(page==3 && reg==0 && count==32);int s=op(2);
 if(s==kStatus_Success)memcpy(lastPCM,values,32);
 return s;
}
static void reset(void)
{
 soundDiagnostics=(soundDiagnostics_t){0};memset(operations,0,sizeof operations);
 noBeep=voice=scanning=false;nextKeyBeepMelody=(int16_t *)MELODY_KEY_BEEP;failOperation=-1;failStatus=-1;irqEnabled=1;fifoValue=0;modeValue=2;
 audioAmpStatusBitfield=0;melody_play=NULL;sine_beep_duration=0;beepTask=(Task_t){0};
 nonVolatileSettings.audioPromptMode=AUDIO_PROMPT_MODE_BEEP;soundBeepVolumeDivider=0;
 amp=0;mux=1;assert(!depth);
}
int main(void)
{
 reset();uiEvent_t event={0};event.keys.event=KEY_MOD_UP;
 keyBeepHandler(&event,false);assert(melody_play==MELODY_KEY_BEEP && soundDiagnostics.keyRequests==1);
 assert(soundDiagnostics.requests==1 && soundDiagnostics.accepted==1);
 soundStopMelody();scanning=true;keyBeepHandler(&event,false);assert(!melody_play);
 reset();soundInitBeepTask();assert(beepTask.Running && beepTask.Handle && soundDiagnostics.createOK==1);
 createResult=errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;soundInitBeepTask();
 assert(!beepTask.Running && !beepTask.Handle && soundDiagnostics.createFailed==1);
 createResult=pdPASS;reset();soundInitBeepTask();timer_beeptask=0;loopTicks=3;
 if(!setjmp(taskExit))createdTask(NULL);
 assert(soundDiagnostics.ticks==3 && soundDiagnostics.timerArms==3 && delays==3);
 reset();soundSetMelody(MELODY_KEY_BEEP);assert(melody_play==MELODY_KEY_BEEP && soundDiagnostics.accepted==1);
 soundTickMelody();assert(sine_beep_freq==MELODY_KEY_BEEP[0] && sine_beep_duration==MELODY_KEY_BEEP[1]);
 assert(amp && !mux && soundDiagnostics.notes==1);
 assert(soundDiagnostics.ampAtNote==1 && soundDiagnostics.muxAtNote==0 && !soundDiagnostics.mutedAtNote);
 beepState_t state={0};soundBeepTaskStep(&state);
 assert(state.open && soundDiagnostics.blocks==1 && operations[0]==1 && operations[4]==1);
 assert(lastPCM[0]==0 && lastPCM[1]==0);bool nonzero=false;for(int i=2;i<32;i++)nonzero|=lastPCM[i]!=0;assert(nonzero);
 sine_beep_freq=0;memset(lastPCM,0xff,32);soundBeepTaskStep(&state);
 for(int i=0;i<32;i++)assert(lastPCM[i]==0);
 soundStopMelody();soundBeepTaskStep(&state);assert(!state.open && !amp && soundDiagnostics.closeOK==1);
 // Each SDK failure code, including positive errors, must prevent downstream IO.
 for(int error=0;error<2;error++)for(int which=0;which<3;which++)
 {
  reset();soundSetMelody(MELODY_KEY_BEEP);soundTickMelody();state=(beepState_t){0};
  failOperation=which;failStatus=error?kStatus_Fail:-1;
  for(int i=0;i<10;i++)soundBeepTaskStep(&state);
  assert(soundDiagnostics.aborts==1 && !melody_play && !amp && state.closePending);
  if(which==0)assert(!state.open && !operations[1] && !operations[2]);
  if(which==1)assert(!operations[2]);
  failOperation=-1;soundBeepTaskStep(&state);assert(!state.closePending && !state.open);
 }
 reset();state=(beepState_t){.open=true};failOperation=3;failStatus=kStatus_Fail;
 soundBeepTaskStep(&state);assert(state.open && state.closePending);
 failOperation=-1;soundBeepTaskStep(&state);assert(!state.open && !state.closePending);
 reset();state=(beepState_t){0};sine_beep_duration=1;fifoValue=0xff;soundBeepTaskStep(&state);
 assert(soundDiagnostics.fifoBusy==1 && !soundDiagnostics.blocks && sine_beep_duration==0);
 reset();state=(beepState_t){0};sine_beep_duration=1;modeValue=0;soundBeepTaskStep(&state);
 assert(!state.open && !operations[1] && soundDiagnostics.openFailed==1);
 reset();state=(beepState_t){0};sine_beep_duration=1;irqEnabled=0;soundBeepTaskStep(&state);assert(!irqEnabled);
 reset();noBeep=true;soundSetMelody(MELODY_KEY_BEEP);assert(!melody_play && soundDiagnostics.channelRejected==1);
 noBeep=false;voice=true;soundSetMelody(MELODY_KEY_BEEP);assert(!melody_play && soundDiagnostics.voiceRejected==1);
 reset();soundSetMelody(MELODY_FM_TALK_PERMIT);state=(beepState_t){0};unsigned ticks=0;
 while(melody_play){soundTickMelody();soundBeepTaskStep(&state);ticks++;assert(ticks<150);}
 assert(soundDiagnostics.notes==5 && soundDiagnostics.blocks==140 && !amp && ticks==141);
 puts("PASS: actual key handler, real melody/PCM/amp/mux, task create/loop/timer, 3-tone 140ms, zero rest, SDK negative/positive faults, failed read/mode/write/close and IRQ restore");
}
