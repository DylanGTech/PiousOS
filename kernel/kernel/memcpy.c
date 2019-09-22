// Compile with GCC -O3 for best performance
// It pretty much entirely negates the need to write these by hand in asm.
#include "kernel/memory.h"

// Default (8-bit, 1 byte at a time)
void * memcpy (void *dest, const void *src, size_t len)
{
  const char *s = (char*)src;
  char *d = (char*)dest;

  while (len--)
  {
    *d++ = *s++;
  }

  return dest;
}