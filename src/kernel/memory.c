/*
   Copyright 2019 Dylan Green

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <efi.h>
#include <efiprot.h>
#include "bootloader/elf.h"

#include "kernel/kernel.h"
#include "kernel/graphics.h"
#include "kernel/memory.h"

MemorySettings mainMemorySettings;

void InitializeMemory(UINTN MapSize, UINTN DescriptorSize, EFI_MEMORY_DESCRIPTOR *Map, UINT32 DescriptorVersion)
{
    mainMemorySettings.memMap = Map;
    mainMemorySettings.memMapDescriptorVersion = DescriptorVersion;
    mainMemorySettings.memMapDescriptorSize = DescriptorSize;
    mainMemorySettings.memMapSize = MapSize;
}

uint64_t AdjustMemMapSize(uint64_t NumberOfNewDescriptors)
{
    size_t numPages = EFI_SIZE_TO_PAGES(mainMemorySettings.memMapSize + (NumberOfNewDescriptors * mainMemorySettings.memMapDescriptorSize));
    size_t originalNumPages = EFI_SIZE_TO_PAGES(mainMemorySettings.memMapSize);

    if(numPages > originalNumPages)
    {
        EFI_MEMORY_DESCRIPTOR * Piece;
        for(Piece = mainMemorySettings.memMap;
            (uint8_t *)Piece < ((uint8_t *)mainMemorySettings.memMap + mainMemorySettings.memMapSize);
            Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t *)Piece + mainMemorySettings.memMapDescriptorSize))
            {
                break;
            }
            if((uint8_t *)Piece == ((uint8_t *)mainMemorySettings.memMap + mainMemorySettings.memMapSize))
        {
            Abort(0xFFFFFFFFFFFFFFFF);
        }
    }
}

uint64_t GetMaxMappedPhysicalAddress(void)
{
    EFI_MEMORY_DESCRIPTOR * Piece;
    uint64_t currentAddress = 0, maxAddress = 0;

    for(Piece = mainMemorySettings.memMap; Piece < (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)mainMemorySettings.memMap + mainMemorySettings.memMapSize); Piece = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)Piece + mainMemorySettings.memMapDescriptorSize))
    {
        currentAddress = Piece->PhysicalStart + Piece->NumberOfPages << EFI_PAGE_SHIFT;
        if(currentAddress > maxAddress)
        {
            maxAddress = currentAddress;
        }
    }
    return maxAddress;
}


uint64_t GetUsableSystemRam(void)
{
    EFI_MEMORY_DESCRIPTOR * Piece;
    uint64_t total = 0;

    for(Piece = mainMemorySettings.memMap; Piece < (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)mainMemorySettings.memMap + mainMemorySettings.memMapSize); Piece = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)Piece + mainMemorySettings.memMapDescriptorSize))
    {
        if(
            (Piece->Type != EfiMemoryMappedIO) &&
            (Piece->Type != EfiMemoryMappedIOPortSpace) &&
            (Piece->Type != EfiPalCode) &&
            (Piece->Type != EfiMaxMemoryType)
        )
        {
            PrintString("Pages found: %d\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, Piece->NumberOfPages);
            total += Piece->NumberOfPages << EFI_PAGE_SHIFT;
        }
    }
    return total;
}

uint64_t GetTotalSystemRam(void)
{
    EFI_MEMORY_DESCRIPTOR * Piece;
    uint64_t total = 0;

    for(Piece = mainMemorySettings.memMap; Piece < (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)mainMemorySettings.memMap + mainMemorySettings.memMapSize); Piece = (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)Piece + mainMemorySettings.memMapDescriptorSize))
    {
        PrintString("Pages found: %d\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, Piece->NumberOfPages);
        total += Piece->NumberOfPages << EFI_PAGE_SHIFT;
    }
    
    return total;
}