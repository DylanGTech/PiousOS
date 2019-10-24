#ifndef _LibC_H
#define _LibC_H 1

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

int memcmp (const void *str1, const void *str2, size_t count);
void * memcpy (void *dest, const void *src, size_t len);
void * memmove (void *dest, const void *src, size_t len);
void * memset (void *dest, const uint8_t val, size_t len);

#endif