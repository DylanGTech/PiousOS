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
#include <efilib.h>
#include <efiprot.h>


void Initialize_System(LOADER_PARAMS* Parameters);
void Abort(uint64_t errorCode);

#endif