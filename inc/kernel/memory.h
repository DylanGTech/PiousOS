#ifndef _Memory_H
#define _Memory_H 1

#include "kernel/kernel.h"


typedef struct MemorySettings {
    UINTN                   memMapSize;              // Size of the memory map (LP->Memory_Map_Size)
    UINTN                   memMapDescriptorSize;    // Size of memory map descriptors (LP->Memory_Map_Descriptor_Size)
    EFI_MEMORY_DESCRIPTOR  *memMap;                  // Pointer to memory map (LP->Memory_Map)
    UINT32                  memMapDescriptorVersion; // Memory map descriptor version
    UINT32                  pad;                     // Pad to multiple of 64 bits
} MemorySettings;

extern MemorySettings mainMemorySettings;

void InitializeMemory(UINTN MapSize, UINTN DescriptorSize, EFI_MEMORY_DESCRIPTOR *Map, UINT32 DescriptorVersion);
uint64_t GetMaxMappedPhysicalAddress(void);
uint64_t GetUsableSystemRam(void);
uint64_t GetTotalSystemRam(void);

#endif