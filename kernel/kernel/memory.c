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
#include <pe.h>
#include "bootloader/elf.h"

#include "kernel/kernel.h"
#include "kernel/graphics.h"
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

uint64_t GetInstalledSystemRam(EFI_CONFIGURATION_TABLE * ConfigurationTables, UINTN NumConfigTables)
{
    uint64_t systemram = 0;
    uint64_t i;
    for(i = 0; i < NumConfigTables; i++)
    {
        if(!(CompareMemory(&ConfigurationTables[i].VendorGuid, &smbios3TableGuid, 16)))
        {
#ifdef DEBUG_PIOUS
            PrintDebugMessage("SMBIOS 3.x table found!\n");
#endif
            SMBIOS_TABLE_3_0_ENTRY_POINT * smb3_entry = (SMBIOS_TABLE_3_0_ENTRY_POINT *)ConfigurationTables[i].VendorTable;
            SMBIOS_STRUCTURE * smb_header = (SMBIOS_STRUCTURE*)smb3_entry->TableAddress;
            uint8_t * smb3_end = (uint8_t *)(smb3_entry->TableAddress + (uint64_t)smb3_entry->TableMaximumSize);

            while((uint8_t*)smb_header < smb3_end)
            {
                if(smb_header->Type == 17) // Memory socket/device
                {
                    uint16_t smb_socket_size = ((SMBIOS_TABLE_TYPE17 *)smb_header)->Size;
                    if(smb_socket_size == 0x7FFF) // Need extended size, which is always given in MB units
                    {
                        uint32_t smb_extended_socket_size = ((SMBIOS_TABLE_TYPE17 *)smb_header)->ExtendedSize;
                        systemram += (uint64_t)smb_extended_socket_size << 20;
                    }
                    else if(smb_socket_size != 0xFFFF)
                    {
                        if(smb_socket_size & 0x8000) // KB units
                        {
                            systemram += (uint64_t)smb_socket_size << 10;
                        }
                        else // MB units
                        {
                            systemram += (uint64_t)smb_socket_size << 20;
                        }
                    }
                    // Otherwise size is unknown (0xFFFF), don't add it.
                }
            }
            break;
        }
        
    }

    if(systemram == 0)
    {
        for(i = 0; i < NumConfigTables; i++)
        {
            if(!CompareMemory(&ConfigurationTables[i].VendorGuid, &smbiosTableGuid, 16))
            {
#ifdef DEBUG_PIOUS
                PrintDebugMessage("SMBIOS table found!\n");
#endif

                SMBIOS_TABLE_ENTRY_POINT * smb_entry = (SMBIOS_TABLE_ENTRY_POINT*)ConfigurationTables[i].VendorTable;
                SMBIOS_STRUCTURE * smb_header = (SMBIOS_STRUCTURE*)((uint64_t)smb_entry->TableAddress);
                uint8_t* smb_end = (uint8_t *)((uint64_t)smb_entry->TableAddress + (uint64_t)smb_entry->TableLength);

                while((uint8_t*)smb_header < smb_end)
                {
                    if(smb_header->Type == 17) // Memory socket/device
                    {
                        uint16_t smb_socket_size = ((SMBIOS_TABLE_TYPE17 *)smb_header)->Size;
                        if(smb_socket_size == 0x7FFF) // Need extended size, which is always given in MB units
                        {
                            uint32_t smb_extended_socket_size = ((SMBIOS_TABLE_TYPE17 *)smb_header)->ExtendedSize;
                            systemram += (uint64_t)smb_extended_socket_size << 20;
                        }
                        else if(smb_socket_size != 0xFFFF)
                        {
                            if(smb_socket_size & 0x8000) // KB units
                            {
                                systemram += (uint64_t)smb_socket_size << 10;
                            }
                            else // MB units
                            {
                                systemram += (uint64_t)smb_socket_size << 20;
                            }
                        }
                        // Otherwise size is unknown (0xFFFF), don't add it.
                    }

                    smb_header = (SMBIOS_STRUCTURE*)((uint64_t)smb_header + smb_header->Length);
                    while(*(uint16_t*)smb_header != 0x0000) // Check for double null, meaning end of string set
                    {
                        smb_header = (SMBIOS_STRUCTURE*)((uint64_t)smb_header + 1);
                    }
                    smb_header = (SMBIOS_STRUCTURE*)((uint64_t)smb_header + 2); // Found end of current structure, move to start of next one
                }

                break;
            }
        }
    }
    if(systemram == 0)
        Abort(0xFFFFFFFFFFFFFFFF);

    return systemram;
}


uint64_t GetVisibleSystemRam(void)
{
  EFI_MEMORY_DESCRIPTOR * Piece;
  uint64_t total = 0;

  for(Piece = mainMemorySettings.MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mainMemorySettings.MemMap + mainMemorySettings.MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)Piece + mainMemorySettings.MemMapDescriptorSize))
  {
    if(
        (Piece->Type != EfiMemoryMappedIO) &&
        (Piece->Type != EfiMemoryMappedIOPortSpace) &&
        (Piece->Type != EfiPalCode) &&
        (Piece->Type != EfiMaxMemoryType)
      )
    {
      total += ((Piece->NumberOfPages) >> EFI_PAGE_SHIFT) + (((Piece->NumberOfPages) & EFI_PAGE_MASK) ? 1 : 0);
    }
  }

  return total;
}
