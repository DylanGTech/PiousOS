// Compile with GCC -O3 for best performance
// It pretty much entirely negates the need to write these by hand in asm.
#include "kernel/memory.h"

int memcmp (const void *str1, const void *str2, size_t count)
{
  const unsigned char *s1 = (unsigned char *)str1;
  const unsigned char *s2 = (unsigned char *)str2;

  while (count-- > 0)
  {
    if (*s1++ != *s2++)
    {
      return s1[-1] < s2[-1] ? -1 : 1;
    }
  }
  return 0;
}

// Equality-only version
int memcmp_eq (const void *str1, const void *str2, size_t count)
{
  const unsigned char *s1 = (unsigned char *)str1;
  const unsigned char *s2 = (unsigned char *)str2;

  while (count-- > 0)
  {
    if (*s1++ != *s2++)
    {
      return -1; // Makes more sense to me if -1 means unequal.
    }
  }
  return 0; // Return 0 if equal to match normal memcmp
}