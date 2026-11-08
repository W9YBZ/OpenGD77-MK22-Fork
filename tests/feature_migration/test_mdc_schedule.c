#include <assert.h>
#include <stdio.h>
#include "functions/mdc1200Schedule.h"
int main(void)
{
 uint8_t tones[26]={0xff};
 const uint32_t clocks[]={24000000,48000000,61440000,60123457};
 for(unsigned c=0;c<4;c++) {
  mdcSchedule_t s;uint32_t origin=UINT32_MAX-100;
  assert(mdcScheduleInit(&s,clocks[c],origin));
  for(unsigned n=1;n<=208;n++) {
   assert((uint32_t)(s.deadline-origin)==(uint32_t)(((uint64_t)n*clocks[c])/1200));
   assert(mdcScheduleTick(&s,s.deadline,false,false,tones)>=0);
  }
  assert(s.done&&!s.failed&&s.symbol==208);
 }
 mdcSchedule_t s;assert(!mdcScheduleInit(&s,0,0));
 assert(mdcScheduleInit(&s,48000000,0));assert(mdcScheduleTick(&s,s.deadline,true,false,tones)<0);
 assert(mdcScheduleInit(&s,48000000,0));assert(mdcScheduleTick(&s,s.deadline+s.lateLimit+1,false,false,tones)<0);
 assert(mdcScheduleInit(&s,48000000,0));assert(mdcScheduleTick(&s,s.deadline,false,true,tones)<0);
 puts("PASS: rational 1200-baud deadlines, 4 clocks, wrap, pending write and late IRQ faults");
}
