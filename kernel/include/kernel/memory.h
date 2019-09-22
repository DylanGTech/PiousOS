#ifndef _Memory_H
#define _Memory_H 1

#include "kernel/kernel.h"


typedef struct MemorySettings {
    UINTN                   MemMapSize;              // Size of the memory map (LP->Memory_Map_Size)
    UINTN                   MemMapDescriptorSize;    // Size of memory map descriptors (LP->Memory_Map_Descriptor_Size)
    EFI_MEMORY_DESCRIPTOR  *MemMap;                  // Pointer to memory map (LP->Memory_Map)
    UINT32                  MemMapDescriptorVersion; // Memory map descriptor version
    UINT32                  Pad;                     // Pad to multiple of 64 bits
} MemorySettings;

extern MemorySettings mainMemorySettings;


void Initialize_Memory(UINTN MapSize, UINTN DescriptorSize, EFI_MEMORY_DESCRIPTOR *Map, UINT32 DescriptorVersion);
uint64_t GetMaxMappedPhysicalAddress(void);



void * memmove (void *dest, const void *src, size_t len);
int memcmp (const void *str1, const void *str2, size_t count);
int memcmp_eq (const void *str1, const void *str2, size_t count);
void * memcpy (void *dest, const void *src, size_t len);
void * memset (void *dest, const uint8_t val, size_t len);

#endif