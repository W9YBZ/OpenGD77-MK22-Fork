#include <assert.h>
#include <stdio.h>
#include "functions/fmFeatures.h"
int main(void)
{
 fmTail_t t = {0};
 assert(fmTailPoll(&t, true, false, PTT_TONE_BEEP, 100) == FM_TAIL_START);
 assert(fmTailPoll(&t, true, false, PTT_TONE_BEEP, 249) == FM_TAIL_WAIT);
 assert(fmTailPoll(&t, true, false, PTT_TONE_BEEP, 250) == FM_TAIL_STOP);
 for (int i=0;i<100;i++) assert(fmTailPoll(&t,true,false,PTT_TONE_BEEP,251+i)==FM_TAIL_NONE);
 for (int mode=0;mode<3;mode++) {
  t=(fmTail_t){0};
  assert(fmTailPoll(&t,false,false,mode,0)==FM_TAIL_NONE); // invalid/automatic session
  assert(fmTailPoll(&t,true,false,mode,1)==FM_TAIL_NONE); // never delayed EOT
  t=(fmTail_t){0};
  assert(fmTailPoll(&t,true,true,mode,0)==FM_TAIL_NONE); // TOT/error
 }
 t=(fmTail_t){0}; assert(fmTailPoll(&t,true,false,PTT_TONE_OFF,0)==FM_TAIL_NONE);
 t=(fmTail_t){0}; assert(fmTailPoll(&t,true,false,PTT_TONE_MDC1200,0)==FM_TAIL_NONE);
 t=(fmTail_t){0}; assert(fmTailPoll(&t,true,false,PTT_TONE_BEEP,0)==FM_TAIL_START);
 assert(fmTailPoll(&t,true,true,PTT_TONE_BEEP,10)==FM_TAIL_STOP); // abort during tail
 t=(fmTail_t){0}; assert(fmTailPoll(&t,true,false,PTT_TONE_BEEP,0)==FM_TAIL_START);
 assert(fmTailPoll(&t,false,false,PTT_TONE_BEEP,10)==FM_TAIL_STOP); // lost TX
 t=(fmTail_t){0}; assert(fmTailPoll(&t,true,false,PTT_TONE_BEEP,UINT32_MAX-99)==FM_TAIL_START);
 assert(fmTailPoll(&t,true,false,PTT_TONE_BEEP,49)==FM_TAIL_WAIT);
 assert(fmTailPoll(&t,true,false,PTT_TONE_BEEP,50)==FM_TAIL_STOP);
 puts("PASS: one tail, 150ms, abort, lost TX, excluded modes, millis wrap");
}
