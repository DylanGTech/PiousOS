// Compile with GCC -O3 for best performance
// It pretty much entirely negates the need to write these by hand in asm.
#include "kernel/memory.h"

// Default (8-bit, 1 byte at a time)
void * memmove (void *dest, const void *src, size_t len)
{
  const char *s = (char *)src;
  char *d = (char *)dest;

  const char *nexts = s + len;
  char *nextd = d + len;

  if (d < s)
  {
    while (d != nextd)
    {
      *d++ = *s++;
    }
  }
  else
  {
    while (nextd != d)
    {
      *--nextd = *--nexts;
    }
  }
  return dest;
}