#include "kernel/libc/string.h"


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