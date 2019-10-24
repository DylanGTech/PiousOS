#include "kernel/libc/string.h"

void * memset (void *dest, const uint8_t val, size_t len)
{
  uint8_t *ptr = (uint8_t*)dest;

  while (len--)
  {
    *ptr++ = val;
  }

  return dest;
}