#ifndef _Kernel_H
#define _Kernel_H

#define PIOUS_MAJOR_VER 0
#define PIOUS_MINOR_VER 0


#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#include "bootloader/bootloader.h"

#include <efi.h>
#include <efiprot.h>

//This assumes unsigned longs and doubles are of the same length (64 bits). Make note of this when porting to other architectures!
typedef union
{
    unsigned long l;
    double d;
} universal_long_t;


void Initialize_System(LOADER_PARAMS* Parameters);
int16_t CompareMemory(const void * addr1, const void * addr2, uint64_t length);
void Abort(uint64_t errorCode);

#endif