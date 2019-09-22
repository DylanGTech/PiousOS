#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <pe.h>
#include "bootloader/elf.h"

#include "kernel/kernel.h"
#include "kernel/memory.h"

MemorySettings mainMemorySettings;

void Initialize_Memory(UINTN MapSize, UINTN DescriptorSize, EFI_MEMORY_DESCRIPTOR *Map, UINT32 DescriptorVersion)
{
    mainMemorySettings.MemMap = Map;
    mainMemorySettings.MemMapDescriptorVersion = DescriptorVersion;
    mainMemorySettings.MemMapDescriptorSize = DescriptorSize;
    mainMemorySettings.MemMapSize = MapSize;
}

uint64_t GetMaxMappedPhysicalAddress(void)
{
  EFI_MEMORY_DESCRIPTOR * Piece;
  uint64_t currentAddress = 0, maxAddress = 0;

  for(Piece = mainMemorySettings.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mainMemorySettings.MemMap + mainMemorySettings.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + mainMemorySettings.MemMapDescriptorSize))
  {
    currentAddress = Piece->PhysicalStart + Piece->NumberOfPages << 12;
    if(currentAddress > maxAddress)
    {
      maxAddress = currentAddress;
    }
  }

  return maxAddress;
}