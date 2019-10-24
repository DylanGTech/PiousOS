#include "kernel/libc/string.h"

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