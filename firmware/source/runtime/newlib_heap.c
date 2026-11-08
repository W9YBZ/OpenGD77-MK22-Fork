/* GNU/newlib runtime adapter. New code under repository license.txt.
 * Redlib/other existing projects do not compile this implementation. */
#if defined(PLATFORM_GD77) && defined(GD77_NEWLIB_BUILD)
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#if defined(__arm__)
#include "MK22F51212.h"
#endif
extern unsigned char _pvHeapStart[],_pvHeapLimit[];
static uintptr_t heapBreak;
void *_sbrk(ptrdiff_t increment)
{
#if defined(__arm__)
 uint32_t mask=__get_PRIMASK();
 __disable_irq();
#endif
 uintptr_t base=(uintptr_t)_pvHeapStart,limit=(uintptr_t)_pvHeapLimit;
 if(!heapBreak)heapBreak=base;
 uintptr_t old=heapBreak;
 int valid=1;
 if(increment>=0) {
  uintptr_t count=(uintptr_t)increment;
  if(count>limit-old)valid=0;else heapBreak+=count;
 } else {
  // Avoid negating PTRDIFF_MIN in signed arithmetic.
  uintptr_t count=(uintptr_t)(-(increment+1))+1U;
  if(count>old-base)valid=0;else heapBreak-=count;
 }
#if defined(__arm__)
 __set_PRIMASK(mask);
#endif
 if(!valid) {errno=ENOMEM;return (void *)-1;}
 return (void *)old;
}
#endif
