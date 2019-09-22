// Compile with GCC -O3 for best performance
// It pretty much entirely negates the need to write these by hand in asm.
#include "kernel/memory.h"

void * memset (void *dest, const uint8_t val, size_t len)
{
  uint8_t *ptr = (uint8_t*)dest;

  while (len--)
  {
    *ptr++ = val;
  }

  return dest;
}