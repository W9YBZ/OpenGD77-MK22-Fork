#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
unsigned char testHeap[64] __attribute__((aligned(8)));
__asm__(".global _pvHeapStart\n.set _pvHeapStart, testHeap\n.global _pvHeapLimit\n.set _pvHeapLimit, testHeap+64\n");
void *_sbrk(ptrdiff_t increment);
int main(void)
{
 assert(_sbrk(0)==testHeap);
 assert(_sbrk(16)==testHeap);
 assert(_sbrk(-8)==testHeap+16);
 assert(_sbrk(56)==testHeap+8);
 errno=0;assert(_sbrk(1)==(void *)-1 && errno==ENOMEM);
 assert(_sbrk(0)==testHeap+64);
 assert(_sbrk(PTRDIFF_MAX)==(void *)-1);
 assert(_sbrk(PTRDIFF_MIN)==(void *)-1);
 assert(_sbrk(-64)==testHeap+64);
 assert(_sbrk(-1)==(void *)-1);
 assert(_sbrk(0)==testHeap);
 puts("PASS: bounded newlib heap, shrink/OOM/extreme increments preserve heap and stack");
}
