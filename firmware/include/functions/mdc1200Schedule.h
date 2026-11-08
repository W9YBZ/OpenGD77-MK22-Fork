/* Pure deadline controller, new code under repository license.txt. */
#ifndef MDC_SCHEDULE_H
#define MDC_SCHEDULE_H
#include <stdbool.h>
#include <stdint.h>
/* No codec or hardware algorithm is shared with an external decoder. */
typedef struct {
 uint32_t base, remainder, fraction, deadline, lateLimit, maxLate;
 uint16_t symbol;
 uint8_t tone;
 bool done, failed;
} mdcSchedule_t;
static inline uint32_t mdcSchedulePeriod(mdcSchedule_t *s)
{
 uint32_t period=s->base;
 s->fraction+=s->remainder;
 if (s->fraction>=1200) {s->fraction-=1200;period++;}
 return period;
}
static inline bool mdcScheduleInit(mdcSchedule_t *s,uint32_t hz,uint32_t now)
{
 if(hz<1200000U || hz>120000000U)return false;
 *s=(mdcSchedule_t){0};s->base=hz/1200;s->remainder=hz%1200;
 s->lateLimit=hz/40000U; // 25us IRQ lateness, experimental engineering bound
 s->deadline=now+mdcSchedulePeriod(s);s->tone=1; // first golden acquisition tone
 return true;
}
/* Called at each absolute symbol boundary. Return 1: change tone, 0: hold/end,
 * -1: fault. A pending write at the next symbol is never silently tolerated. */
static inline int mdcScheduleTick(mdcSchedule_t *s,uint32_t now,
                                  bool pending,bool fault,const uint8_t tones[26])
{
 if(s->done||s->failed)return s->failed ? -1 : 0;
 int32_t late=(int32_t)(now-s->deadline);
 if(late>=0 && (uint32_t)late>s->maxLate)s->maxLate=late;
 if(late<0 || (uint32_t)late>s->lateLimit || pending || fault) {
  s->failed=true;return -1;
 }
 if(++s->symbol==208) {s->done=true;return 0;}
 uint8_t next=(tones[s->symbol/8]>>(7-s->symbol%8))&1U;
 s->deadline+=mdcSchedulePeriod(s);
 int changed=next!=s->tone;s->tone=next;return changed;
}
#endif
