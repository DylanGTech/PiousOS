// From KNNSpeed's "Simple UEFI Bootloader":
// https://github.com/KNNSpeed/Simple-UEFI-Bootloader
// V2.2, June 1, 2019

// Heavily Modified by Dylan Green


#include <efi.h>
#include <efilib.h>
#include <efiprot.h>

#ifdef x86_64
#include <pe.h>
#elif aarch64
#include "bootloader/pe.h"
#endif

#include "bootloader/elf.h"

#include "bootloader/bootloader.h"


EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {

   InitializeLib(ImageHandle, SystemTable);

   Print(L"Hello World!\r\n");

   GPU_CONFIG *Graphics;
   EFI_STATUS Status = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, sizeof(GPU_CONFIG), (void**)&Graphics);
   if(EFI_ERROR(Status))
   {
      Print(L"Graphics AllocatePool error. 0x%llx\r\n", Status);
      return Status;
   }
   
   Status = InitUEFI_GOP(ImageHandle, Graphics);
   if(EFI_ERROR(Status))
   {
      Print(L"InitUEFI_GOP error. 0x%llx\r\n", Status);
      return Status;
   }

   Status = BootKernel(ImageHandle, Graphics, ST->ConfigurationTable, ST->NumberOfTableEntries, ST->Hdr.Revision);

   while(1) ;
   return Status;
}



EFI_STATUS BootKernel(EFI_HANDLE ImageHandle, GPU_CONFIG * Graphics, EFI_CONFIGURATION_TABLE * SysCfgTables, UINTN NumSysCfgTables, UINT32 UEFIVer)
{
    Print(L"Booting kernel\r\n");

#ifdef x86_64
  
  UINT64 reg;

  asm volatile("mov %%cr0, %[dest]"
      : [dest] "=r" (reg) // Outputs
      : // Inputs
      : // Clobbers
  );

  //Turn off the paging bit if it's on from UEFI
  if(reg & (1 << 16))
  {
      reg ^= (1 << 16);
      asm volatile("mov %[dest], %%cr0"
          : // Outputs
          : [dest] "r" (reg) // Inputs
          : // Clobbers
      );
  }

  
#elif aarch64
    //Do anything paging-related here
#endif


    EFI_STATUS BootStatus;

    EFI_PHYSICAL_ADDRESS KernelBaseAddress = 0;
    UINTN KernelPages = 0;

	  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

    BootStatus = uefi_call_wrapper(ST->BootServices->OpenProtocol, 6, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if(EFI_ERROR(BootStatus))
    {
       Print(L"LoadedImage OpenProtocol error. 0x%llx\r\n", BootStatus);
       return BootStatus;
    }


    CHAR16 * ESPRootTemp = DevicePathToStr(DevicePathFromHandle(LoadedImage->DeviceHandle));
    UINT64 ESPRootSize = StrSize(ESPRootTemp);

    CHAR16 * ESPRoot;

    BootStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, ESPRootSize, (void**)&ESPRoot);
    if(EFI_ERROR(BootStatus))
    {
        Print(L"ESPRoot AllocatePool error. 0x%llx\r\n", BootStatus);
        return BootStatus;
    }

    CopyMem(ESPRoot, ESPRootTemp, ESPRootSize);

    BootStatus = uefi_call_wrapper(BS->FreePool, 1, ESPRootTemp);
    if(EFI_ERROR(BootStatus))
    {
        Print(L"Error freeing ESPRootTemp pool. 0x%llx\r\n", BootStatus);
        return BootStatus;
    }

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;

    BootStatus = uefi_call_wrapper(ST->BootServices->OpenProtocol, 6, LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if(EFI_ERROR(BootStatus))
    {
        Print(L"FileSystem OpenProtocol error. 0x%llx\r\n", BootStatus);
        return BootStatus;
    }

    EFI_FILE *CurrentDriveRoot;

    BootStatus = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &CurrentDriveRoot);
    if(EFI_ERROR(BootStatus))
    {
        Print(L"OpenVolume error. 0x%llx\r\n", BootStatus);
        return BootStatus;
    }

    CHAR16 * KernelPath = L"\\Pious\\Kernel.exe";
    //UINT64 KernelPathLen = 17;x
    UINT64 KernelPathSize = (17 + 1) << 1;


    EFI_FILE *KernelFile;

    // Open the kernel file from current drive root and point to it with KernelFile
	  BootStatus = uefi_call_wrapper(CurrentDriveRoot->Open, 5, CurrentDriveRoot, &KernelFile, KernelPath, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
	  if (EFI_ERROR(BootStatus))
    {
		    Print(L"%s file is missing\r\n", KernelPath);
		    return BootStatus;
	  }


    // Default ImageBase for 64-bit PE DLLs
    EFI_PHYSICAL_ADDRESS Header_memory = 0x40000000;

    
    UINTN FileInfoSize = 0;
    EFI_FILE_INFO *FileInfo;

    BootStatus = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    // GetInfo will intentionally error out and provide the correct fileinfosize value


    BootStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, FileInfoSize, (void**)&FileInfo); // Reserve memory for file info/attributes and such, to prevent it from getting run over
    if(EFI_ERROR(BootStatus))
    {
        Print(L"FileInfo AllocatePool error. 0x%llx\r\n", BootStatus);
        return BootStatus;
    }
    
    // Actually get the metadata
    BootStatus = uefi_call_wrapper(KernelFile->GetInfo, 4, KernelFile, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if(EFI_ERROR(BootStatus))
    {
        Print(L"GetInfo error. 0x%llx\r\n", BootStatus);
        return BootStatus;
    }

    // Read file header
    UINTN size = sizeof(IMAGE_DOS_HEADER);
    IMAGE_DOS_HEADER DOSheader;

    BootStatus = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &size, &DOSheader);
    if(EFI_ERROR(BootStatus))
    {
        Print(L"DOSheader read error. 0x%llx\r\n", BootStatus);
        return BootStatus;
    }


#ifdef x86_64
    // For the entry point jump, we need to know if the file uses ms_abi (is a PE image) or sysv_abi (*NIX image) calling convention
    UINT8 KernelisPE = 0;
#endif

    //----------------------------------------------------------------------------------------------------------------------------------
    //  64-Bit ELF Loader
    //----------------------------------------------------------------------------------------------------------------------------------

    // Slightly less terrible way of doing this; just a placeholder.
    BootStatus = uefi_call_wrapper(KernelFile->SetPosition, 2, KernelFile, 0);
    if(EFI_ERROR(BootStatus))
    {
      Print(L"Reset SetPosition error (ELF). 0x%llx\r\n", BootStatus);
      return BootStatus;
    }

    Elf64_Ehdr ELF64header;
    size = sizeof(ELF64header); // This works because it's not a pointer

    BootStatus = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &size, &ELF64header);
    if(EFI_ERROR(BootStatus))
    {
      Print(L"Header read error (ELF). 0x%llx\r\n", BootStatus);
      return BootStatus;
    }

    if(Compare(&ELF64header.e_ident[EI_MAG0], ELFMAG, SELFMAG)) // Check for \177ELF (hex: \xfELF)
    {
      // ELF!


      // Check if 64-bit
#ifdef x86_64
      if(ELF64header.e_ident[EI_CLASS] == ELFCLASS64 && ELF64header.e_machine == EM_X86_64)
#elif aarch64
      if(ELF64header.e_ident[EI_CLASS] == ELFCLASS64 && ELF64header.e_machine == EM_AARCH64)
#endif
      {

        UINT64 i; // Iterator
        UINT64 virt_size = 0; // Virtual address max
        UINT64 virt_min = ~0ULL; // Minimum virtual address for page number calculation, -1 wraps around to max 64-bit number
        UINT64 Numofprogheaders = (UINT64)ELF64header.e_phnum;
        size = Numofprogheaders*(UINT64)ELF64header.e_phentsize; // Size of all program headers combined

        Elf64_Phdr * program_headers_table;

        BootStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiBootServicesData, size, (void**)&program_headers_table);
        if(EFI_ERROR(BootStatus))
        {
          Print(L"Program headers table AllocatePool error. 0x%llx\r\n", BootStatus);
          return BootStatus;
        }

        BootStatus = uefi_call_wrapper(KernelFile->SetPosition, 2, KernelFile, ELF64header.e_phoff); // Go to program headers
        if(EFI_ERROR(BootStatus))
        {
          Print(L"Error setting file position for mapping (ELF). 0x%llx\r\n", BootStatus);
          return BootStatus;
        }
        BootStatus = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &size, &program_headers_table[0]); // Run right over the section table, it should be exactly the size to hold this data
        if(EFI_ERROR(BootStatus))
        {
          Print(L"Error reading program headers (ELF). 0x%llx\r\n", BootStatus);
          return BootStatus;
        }

        // Only want to include PT_LOAD segments
        for(i = 0; i < Numofprogheaders; i++) // Go through each section of the "sections" section to get the address boundary of the last section
        {
          Elf64_Phdr *specific_program_header = &program_headers_table[i];
          if(specific_program_header->p_type == PT_LOAD)
          {

            virt_size = (virt_size > (specific_program_header->p_vaddr + specific_program_header->p_memsz) ? virt_size: (specific_program_header->p_vaddr + specific_program_header->p_memsz));
            virt_min = (virt_min < (specific_program_header->p_vaddr) ? virt_min: (specific_program_header->p_vaddr));
          }
        }

        // Virt_min is technically also the base address of the loadable segments
        UINT64 pages = EFI_SIZE_TO_PAGES(virt_size - virt_min); //To get number of pages (typically 4KB per), rounded up
        KernelPages = pages;


        EFI_PHYSICAL_ADDRESS AllocatedMemory = 0x40000000; // 1 GiB


        BootStatus = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &AllocatedMemory);

        if(EFI_ERROR(BootStatus))
        {
          Print(L"Could not allocate pages for ELF program segments. Error code: 0x%llx\r\n", BootStatus);
          return BootStatus;
        }


        // Zero the allocated pages
        ZeroMem((VOID*)AllocatedMemory, (pages << EFI_PAGE_SHIFT));

        // If that memory isn't actually free due to weird firmware behavior...
        // Iterate through the entirety of what was just allocated and check to make sure it's all zeros
        // Start buggy firmware workaround
        if(VerifyZeroMem(pages << EFI_PAGE_SHIFT, AllocatedMemory))
        {

          // From UEFI Specification 2.7, Errata A (http://www.uefi.org/specifications):
          // MemoryType values in the range 0x80000000..0xFFFFFFFF are reserved for use by
          // UEFI OS loaders that are provided by operating system vendors.
          // Compare what's there with the kernel file's first bytes; the system might have been reset and the non-zero
          // memory is what remains of last time. This can be safely overwritten to avoid cluttering up system RAM.

          // Sure hope there aren't any other page-aligned kernel images floating around in memory marked as free
          // Good thing we know what to expect!

          if(Compare((EFI_PHYSICAL_ADDRESS*)AllocatedMemory, ELFMAG, SELFMAG))
          {
            // Do nothing, we're fine
          }
          else // Not our remains, proceed with discovery of viable memory address
          {
            // Free the pages (well, return them to the system as they were...)
            BootStatus = uefi_call_wrapper(BS->FreePages, 2, AllocatedMemory, pages);
            if(EFI_ERROR(BootStatus))
            {
              Print(L"Could not free pages for ELF sections. Error code: 0x%llx\r\n", BootStatus);
              return BootStatus;
            }

            // NOTE: CANNOT create an array of all compatible free addresses because the array takes up memory. So does the memory map.
            // This results in a paradox, so we need to scan the memory map every time we need to find a new address...

            // It appears that AllocateAnyPages uses a "MaxAddress" approach. This will go bottom-up instead.
            EFI_PHYSICAL_ADDRESS NewAddress = 0; // Start at zero
            EFI_PHYSICAL_ADDRESS OldAllocatedMemory = AllocatedMemory;

            BootStatus = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &NewAddress); // Need to check 0x0
            while(BootStatus != EFI_SUCCESS)
            { // Keep checking free memory addresses until one works

              if(BootStatus == EFI_NOT_FOUND)
              {
                // 0's not a good address (not enough contiguous pages could be found), get another one
                NewAddress = ActuallyFreeAddress(pages, NewAddress);
                // Make sure the new address isn't the known bad one
                if(NewAddress == OldAllocatedMemory)
                {
                  // Get a new address if it is
                  NewAddress = ActuallyFreeAddress(pages, NewAddress);
                }
                // Address can be > 4GB
              }
              else if(EFI_ERROR(BootStatus))
              {
                Print(L"Could not get an address for ELF pages. Error code: 0x%llx\r\n", BootStatus);
                return BootStatus;
              }

              if(NewAddress == ~0ULL)
              {
                // If you get this, you had no memory free anywhere.
                Print(L"No memory marked as EfiConventionalMemory...\r\n");
                return BootStatus;
              }

              // Allocate the new address
              BootStatus = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &NewAddress);
              // This loop shouldn't run more than once, but in the event something is at 0x0 we need to
              // leave the loop with an allocated address

            }

            // Got a new address that's been allocated--save it
            AllocatedMemory = NewAddress;

            // Verify it's empty
            while((NewAddress != ~0ULL) && VerifyZeroMem(pages << EFI_PAGE_SHIFT, AllocatedMemory)) // Loop this in case the firmware is really screwed
            { // It's not empty :(

              // Sure hope there aren't any other page-aligned kernel images floating around in memory marked as free
              if(Compare((EFI_PHYSICAL_ADDRESS*)AllocatedMemory, ELFMAG, SELFMAG))
              {
                // Do nothing, we're fine
                break;
              }
              else
              { // Gotta keep looking for a good memory address

                // It's not actually free...
                BootStatus = uefi_call_wrapper(BS->FreePages, 2, AllocatedMemory, pages);
                if(EFI_ERROR(BootStatus))
                {
                  Print(L"Could not free pages for ELF sections (loop). Error code: 0x%llx\r\n", BootStatus);
                  return BootStatus;
                }

                // Allocate a new address
                BootStatus = EFI_NOT_FOUND;
                while((BootStatus != EFI_SUCCESS) && (NewAddress != ~0ULL))
                {
                  if(BootStatus == EFI_NOT_FOUND)
                  {
                    // Get an address (ideally, this should be very rare)
                    NewAddress = ActuallyFreeAddress(pages, NewAddress);
                    // Make sure the new address isn't the known bad one
                    if(NewAddress == OldAllocatedMemory)
                    {
                      // Get a new address if it is
                      NewAddress = ActuallyFreeAddress(pages, NewAddress);
                    }
                    // Address can be > 4GB
                    // This loop will run until we get a good address (shouldn't be more than once, if ever)
                  }
                  else if(EFI_ERROR(BootStatus))
                  {
                    // EFI_OUT_OF_RESOURCES means the firmware's just not gonna load anything.
                    Print(L"Could not get an address for ELF pages (loop). Error code: 0x%llx\r\n", BootStatus);
                    return BootStatus;
                  }
                  // NOTE: The number of times the message "No more free addresses" pops up
                  // helps indicate which NewAddress assignment hit the end.

                  BootStatus = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &NewAddress);
                } // loop

                // It's a new address
                AllocatedMemory = NewAddress;

                // Verify new address is empty (in loop), if not then free it and try again.
              } // else
            } // End VerifyZeroMem while loop

            // Ran out of easy addresses, time for a more thorough check
            // Hopefully no one ever gets here
            if(AllocatedMemory == ~0ULL)
            { // NewAddress is also -1
              NewAddress = ActuallyFreeAddress(pages, 0); // Start from the first suitable EfiConventionalMemory address.
              // Allocate the page's address
              BootStatus = EFI_NOT_FOUND;
              while(BootStatus != EFI_SUCCESS)
              {
                if(BootStatus == EFI_NOT_FOUND)
                {
                  // Nope, get another one
                  NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                  // Make sure the new address isn't the known bad one
                  if(NewAddress == OldAllocatedMemory)
                  {
                    // Get a new address if it is
                    NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                  }
                  // Adresses very well might be > 4GB with the filesizes these are allowed to be
                }
                else if(EFI_ERROR(BootStatus))
                {
                  Print(L"Could not get an address for ELF pages by page. Error code: 0x%llx\r\n", BootStatus);
                  return BootStatus;
                }

                if(NewAddress == ~0ULL)
                {
                  // If you somehow get this, you really had no memory free anywhere.
                  Print(L"Hmm... How did you get here?\r\n");
                  return BootStatus;
                }

                BootStatus = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &NewAddress);
              }

              AllocatedMemory = NewAddress;

              while(VerifyZeroMem(pages << EFI_PAGE_SHIFT, AllocatedMemory))
              {
                // Sure hope there aren't any other page-aligned kernel images floating around in memory marked as free
                if(Compare((EFI_PHYSICAL_ADDRESS*)AllocatedMemory, ELFMAG, SELFMAG))
                {

                  break;
                }
                else
                {

                  // It's not actually free...
                  BootStatus = uefi_call_wrapper(BS->FreePages, 2, AllocatedMemory, pages);
                  if(EFI_ERROR(BootStatus))
                  {
                    Print(L"Could not free pages for ELF sections by page (loop). Error code: 0x%llx\r\n", BootStatus);
                    return BootStatus;
                  }

                  BootStatus = EFI_NOT_FOUND;
                  while(BootStatus != EFI_SUCCESS)
                  {
                    if(BootStatus == EFI_NOT_FOUND)
                    {
                      // Nope, get another one
                      NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                      // Make sure the new address isn't the known bad one
                      if(NewAddress == OldAllocatedMemory)
                      {
                        // Get a new address if it is
                        NewAddress = ActuallyFreeAddressByPage(pages, NewAddress);
                      }
                      // Address can be > 4GB
                    }
                    else if(EFI_ERROR(BootStatus))
                    {
                      Print(L"Could not get an address for ELF pages by page (loop). Error code: 0x%llx\r\n", BootStatus);
                      return BootStatus;
                    }

                    if(AllocatedMemory == ~0ULL)
                    {
                      // Well, darn. Something's up with the system memory.
                      return BootStatus;
                    }

                    BootStatus = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, &NewAddress);
                  } // loop

                  AllocatedMemory = NewAddress;

                } // else
              } // end ByPage VerifyZeroMem loop
            } // End "big guns"

            // Got a good address!
          } // End discovery of viable memory address (else)
          // Can move on now
        } // End VerifyZeroMem buggy firmware workaround (outermost if)
        else
        {

        }


        // No need to copy headers to memory for ELFs, just the program itself
        // Only want to include PT_LOAD segments
        for(i = 0; i < Numofprogheaders; i++) // Load sections into memory
        {
          Elf64_Phdr *specific_program_header = &program_headers_table[i];
          UINTN RawDataSize = specific_program_header->p_filesz; // 64-bit ELFs can have 64-bit file sizes!
          EFI_PHYSICAL_ADDRESS SectionAddress = AllocatedMemory + (specific_program_header->p_vaddr - program_headers_table[0].p_vaddr); // 64-bit ELFs use 64-bit addressing!

          if(specific_program_header->p_type == PT_LOAD)
          {

            BootStatus = uefi_call_wrapper(KernelFile->SetPosition, 2, KernelFile, specific_program_header->p_offset); // p_offset is a UINT64 relative to the beginning of the file, just like Read() expects!
            if(EFI_ERROR(BootStatus))
            {
              Print(L"Program segment SetPosition error (ELF). 0x%llx\r\n", BootStatus);
              return BootStatus;
            }

            if(RawDataSize != 0) // Apparently some UEFI implementations can't deal with reading 0 byte sections
            {
              BootStatus = uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &RawDataSize, (EFI_PHYSICAL_ADDRESS*)SectionAddress);
              if(EFI_ERROR(BootStatus))
              {
                Print(L"Program segment read error (ELF). 0x%llx\r\n", BootStatus);
                return BootStatus;
              }
            }
          }
        }

        EFI_PHYSICAL_ADDRESS startAddress = program_headers_table[0].p_vaddr;
        // Done with program_headers_table
        if(program_headers_table)
        {
          BootStatus = uefi_call_wrapper(BS->FreePool, 1, program_headers_table);
          if(EFI_ERROR(BootStatus))
          {
            Print(L"Error freeing program headers table pool. 0x%llx\r\n", BootStatus);
          }
        }

        // Link kernel with -static-pie and there's no need for relocations beyond the base-relative ones just done. YUS!

        // e_entry should be a 64-bit relative memory address, and gives the kernel's entry point
        KernelBaseAddress = AllocatedMemory;
        Header_memory = ELF64header.e_entry; //AllocatedMemory + ELF64header.e_entry;

        MapVirtualPages(AllocatedMemory, startAddress, pages, 0x3, ST);
        
		    //uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAddress, EfiLoaderData, pages, (UINTN *)startAddress);

        Print(L"FINISHED MAP!!!\r\n");
        /*
        asm volatile("mov %[dest], %%cr0"
            : // Outputs
            : [dest] "r" (reg ^ (1 << 16)) // Inputs
            : // Clobbers
        );
        */
        // Loaded! On to memorymap and exitbootservices...
        // NOTE: Executable entry point is now defined in Header_memory's contained address, which is AllocatedMemory + ELF64header.e_entry

      }
      else
      {
        BootStatus = EFI_INVALID_PARAMETER;
#ifdef x86_64
        Print(L"Hey! 64-bit (x86_64) ELFs only.\r\n");
#elif aarch64
        Print(L"Hey! 64-bit (aarch64) ELFs only.\r\n");
#endif
        return BootStatus;
      }
  }

  // Reserve memory for the loader block
  LOADER_PARAMS * Loader_block;
  BootStatus = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, sizeof(LOADER_PARAMS), (void**)&Loader_block);
  if(EFI_ERROR(BootStatus))
  {
    Print(L"Error allocating loader block pool. Error: 0x%llx\r\n", BootStatus);
    return BootStatus;
  }

 //----------------------------------------------------------------------------------------------------------------------------------
 //  Get Memory Map and Exit Boot Services
 //----------------------------------------------------------------------------------------------------------------------------------

  // UINTN is the largest uint type supported. For x86_64 and AARCH64, this is uint64_t
  UINTN MemMapSize = 0, MemMapKey, MemMapDescriptorSize;
  UINT32 MemMapDescriptorVersion;
  EFI_MEMORY_DESCRIPTOR * MemMap = NULL;

  // Get memory map and exit boot services
  BootStatus = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  if(BootStatus == EFI_BUFFER_TOO_SMALL)
  {
    BootStatus = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, MemMapSize, (void **)&MemMap); // Allocate pool for MemMap (it should always be resident in memory)
    if(EFI_ERROR(BootStatus)) // Error! Wouldn't be safe to continue.
    {
      Print(L"MemMap AllocatePool error. 0x%llx\r\n", BootStatus);
      return BootStatus;
    }
    BootStatus = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  }
  

  BootStatus = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MemMapKey);

  if(EFI_ERROR(BootStatus)) // Error! EFI_INVALID_PARAMETER, MemMapKey is incorrect
  {

    BootStatus = uefi_call_wrapper(BS->FreePool, 1, MemMap);

    MemMapSize = 0;
    BootStatus = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
    if(BootStatus == EFI_BUFFER_TOO_SMALL)
    {
      BootStatus = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, MemMapSize, (void **)&MemMap);
      if(EFI_ERROR(BootStatus)) // Error! Wouldn't be safe to continue.
      {
        Print(L"MemMap AllocatePool error #2. 0x%llx\r\n", BootStatus);
        return BootStatus;
      }
      BootStatus = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
    }
  
    
    BootStatus = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MemMapKey);

  }
  
  // This applies to both the simple and larger versions of the above.
  if(EFI_ERROR(BootStatus))
  {
    Print(L"Could not exit boot services... 0x%llx\r\n", BootStatus);
    BootStatus = uefi_call_wrapper(BS->FreePool, 1, MemMap);
    if(EFI_ERROR(BootStatus)) // Error! Wouldn't be safe to continue.
    {
      Print(L"Error freeing MemMap pool. 0x%llx\r\n", BootStatus);
    }
    Print(L"MemMapSize: %llx, MemMapKey: %llx\r\n", MemMapSize, MemMapKey);
    Print(L"DescriptorSize: %llx, DescriptorVersion: %x\r\n", MemMapDescriptorSize, MemMapDescriptorVersion);
    return BootStatus;
  }

  //----------------------------------------------------------------------------------------------------------------------------------
  //  Entry Point Jump
  //----------------------------------------------------------------------------------------------------------------------------------

  // This shouldn't modify the memory map.
  Loader_block->UEFI_Version = UEFIVer;
  Loader_block->Bootloader_MajorVersion = BOOTLOADER_MAJOR_VER;
  Loader_block->Bootloader_MinorVersion = BOOTLOADER_MINOR_VER;

  Loader_block->Memory_Map_Descriptor_Version = MemMapDescriptorVersion;
  Loader_block->Memory_Map_Descriptor_Size = MemMapDescriptorSize;
  Loader_block->Memory_Map = MemMap;
  Loader_block->Memory_Map_Size = MemMapSize;

  Loader_block->Kernel_BaseAddress = KernelBaseAddress;
  Loader_block->Kernel_Pages = KernelPages;

  Loader_block->ESP_Root_Device_Path = ESPRoot;
  Loader_block->ESP_Root_Size = ESPRootSize;
  Loader_block->Kernel_Path = KernelPath;
  Loader_block->Kernel_Path_Size = KernelPathSize;

  Loader_block->RTServices = RT;
  Loader_block->GPU_Configs = Graphics;
  Loader_block->FileMeta = FileInfo;

  Loader_block->ConfigTables = SysCfgTables;
  Loader_block->Number_of_ConfigTables = NumSysCfgTables;

#ifdef x86_64

  // Jump to entry point, and WE ARE LIVE!!
  if(KernelisPE)
  {
    typedef void (__attribute__((ms_abi)) *EntryPointFunction)(LOADER_PARAMS * LP); // Placeholder names for jump
    EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(Header_memory);
    EntryPointPlaceholder(Loader_block);
  }
  else
  {
    typedef void (__attribute__((sysv_abi)) *EntryPointFunction)(LOADER_PARAMS * LP); // Placeholder names for jump
    EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(Header_memory);
    EntryPointPlaceholder(Loader_block);
  }
#elif aarch64


  
  typedef void (*EntryPointFunction)(LOADER_PARAMS * LP); // Placeholder names for jump
  EntryPointFunction EntryPointPlaceholder = (EntryPointFunction)(Header_memory);
  
  EntryPointPlaceholder(Loader_block);
#endif

  // Should never get here
  return BootStatus;
}




UINT8 Compare(const void* firstitem, const void* seconditem, UINT64 comparelength)
{
  // Using const since this is a read-only operation: absolutely nothing should be changed here.
  const UINT8 *one = firstitem, *two = seconditem;
  for (UINT64 i = 0; i < comparelength; i++)
  {
    if(one[i] != two[i])
    {
      return 0;
    }
  }
  return 1;
}

UINT8 VerifyZeroMem(UINT64 NumBytes, UINT64 BaseAddr) // BaseAddr is a 64-bit unsigned int whose value is the memory address
{
  for(UINT64 i = 0; i < NumBytes; i++)
  {
    if(*(((UINT8*)BaseAddr) + i) != 0)
    {
      return 1;
    }
  }
  return 0;
}


EFI_PHYSICAL_ADDRESS ActuallyFreeAddress(UINT64 pages, EFI_PHYSICAL_ADDRESS OldAddress)
{
  EFI_STATUS memmap_status;
  UINTN MemMapSize = 0, MemMapKey, MemMapDescriptorSize;
  UINT32 MemMapDescriptorVersion;
  EFI_MEMORY_DESCRIPTOR * MemMap = NULL;
  EFI_MEMORY_DESCRIPTOR * Piece;

  memmap_status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  if(memmap_status == EFI_BUFFER_TOO_SMALL)
  {
    memmap_status = uefi_call_wrapper(BS->AllocatePool, 3, EfiBootServicesData, MemMapSize, (void **)&MemMap); // Allocate pool for MemMap
    if(EFI_ERROR(memmap_status)) // Error! Wouldn't be safe to continue.
    {
      Print(L"ActuallyFreeAddress MemMap AllocatePool error. 0x%llx\r\n", memmap_status);
      return ~0ULL;
    }
    memmap_status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  }
  if(EFI_ERROR(memmap_status))
  {
    Print(L"Error getting memory map for ActuallyFreeAddress. 0x%llx\r\n", memmap_status);
    return ~0ULL;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Piece + MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages) && (Piece->PhysicalStart > OldAddress))
    {
      break;
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize))
  {
    return ~0ULL;
  }

  memmap_status = uefi_call_wrapper(BS->FreePool, 1, MemMap);
  if(EFI_ERROR(memmap_status))
  {
    Print(L"Error freeing ActuallyFreeAddress memmap pool. 0x%llx\r\n", memmap_status);
  }

  return Piece->PhysicalStart;
}

EFI_PHYSICAL_ADDRESS ActuallyFreeAddressByPage(UINT64 pages, EFI_PHYSICAL_ADDRESS OldAddress)
{
  EFI_STATUS memmap_status;
  UINTN MemMapSize = 0, MemMapKey, MemMapDescriptorSize;
  UINT32 MemMapDescriptorVersion;
  EFI_MEMORY_DESCRIPTOR * MemMap = NULL;
  EFI_MEMORY_DESCRIPTOR * Piece;
  EFI_PHYSICAL_ADDRESS PhysicalEnd;
  EFI_PHYSICAL_ADDRESS DiscoveredAddress;

  memmap_status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  if(memmap_status == EFI_BUFFER_TOO_SMALL)
  {
    memmap_status = uefi_call_wrapper(BS->AllocatePool, 3, EfiBootServicesData, MemMapSize, (void **)&MemMap); // Allocate pool for MemMap
    if(EFI_ERROR(memmap_status)) // Error! Wouldn't be safe to continue.
    {
      Print(L"ActuallyFreeAddressByPage MemMap AllocatePool error. 0x%llx\r\n", memmap_status);
      return ~0ULL;
    }
    memmap_status = uefi_call_wrapper(BS->GetMemoryMap, 5, &MemMapSize, MemMap, &MemMapKey, &MemMapDescriptorSize, &MemMapDescriptorVersion);
  }
  if(EFI_ERROR(memmap_status))
  {
    Print(L"Error getting memory map for ActuallyFreeAddressByPage. 0x%llx\r\n", memmap_status);
    return ~0ULL;
  }

  // Multiply NumberOfPages by EFI_PAGE_SIZE to get the end address... which should just be the start of the next section.
  // Check for EfiConventionalMemory in the map
  for(Piece = MemMap; Piece < (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize); Piece = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Piece + MemMapDescriptorSize))
  {
    // Within each compatible EfiConventionalMemory, look for space
    if((Piece->Type == EfiConventionalMemory) && (Piece->NumberOfPages >= pages))
    {
      PhysicalEnd = Piece->PhysicalStart + (Piece->NumberOfPages << EFI_PAGE_SHIFT) - EFI_PAGE_MASK; // Get the end of this range, and use it to set a bound on the range (define a max returnable address).
      // (pages*EFI_PAGE_SIZE) or (pages << EFI_PAGE_SHIFT) gives the size the kernel would take up in memory
      if((OldAddress >= Piece->PhysicalStart) && ((OldAddress + (pages << EFI_PAGE_SHIFT)) < PhysicalEnd)) // Bounds check on OldAddress
      {
        // Return the next available page's address in the range. We need to go page-by-page for the really buggy systems.
        DiscoveredAddress = OldAddress + EFI_PAGE_SIZE; // Left shift EFI_PAGE_SIZE by 1 or 2 to check every 0x10 or 0x100 pages (must also modify the above PhysicalEnd bound check)
        break;
        // If PhysicalEnd == OldAddress, we need to go to the next EfiConventionalMemory range
      }
      else if(Piece->PhysicalStart > OldAddress) // Try a new range
      {
        DiscoveredAddress = Piece->PhysicalStart;
        break;
      }
    }
  }

  // Loop ended without a DiscoveredAddress
  if(Piece >= (EFI_MEMORY_DESCRIPTOR*)((UINT8*)MemMap + MemMapSize))
  {
    // Return address -1, which will cause AllocatePages to fail
    return ~0ULL;
  }

  memmap_status = uefi_call_wrapper(BS->FreePool, 1, MemMap);
  if(EFI_ERROR(memmap_status))
  {
    Print(L"Error freeing ActuallyFreeAddressByPage memmap pool. 0x%llx\r\n", memmap_status);
  }

  return DiscoveredAddress;
}

// This array is a global variable so that it can be made static, which helps prevent a stack overflow if it ever needs to lengthen.
STATIC CONST CHAR16 PxFormats[5][17] = {
    L"RGBReserved 8Bpp",
    L"BGRReserved 8Bpp",
    L"PixelBitMask    ",
    L"PixelBltOnly    ",
    L"PixelFormatMax  "
};

EFI_STATUS InitUEFI_GOP(EFI_HANDLE ImageHandle, GPU_CONFIG * Graphics)
{ // Declaring a pointer only allocates 8 bytes (64-bit) for that pointer. Buffers must be manually allocated memory via AllocatePool and then freed with FreePool when done with.

  Graphics->NumberOfFrameBuffers = 0;

  EFI_STATUS GOPStatus;

  UINT64 GOPInfoSize;
  UINT32 mode;
  UINTN NumHandlesInHandleBuffer = 0; // Number of discovered graphics handles (GPUs)
  UINTN NumName2Handles = 0;
  UINTN NumDevPathHandles = 0;
  UINT64 DevNum = 0;
  EFI_INPUT_KEY Key;

  Key.UnicodeChar = 0;

  // Vendors go all over the place with these...
  CHAR8 LanguageToUse[6] = {'e','n','-','U','S','\0'};
  CHAR8 LanguageToUse2[3] = {'e','n','\0'};
  CHAR8 LanguageToUse3[4] = {'e','n','g','\0'};

  CHAR16 DefaultDriverDisplayName[15] = L"No Driver Name";
  CHAR16 DefaultControllerDisplayName[19] = L"No Controller Name";
  CHAR16 DefaultChildDisplayName[14] = L"No Child Name";

  CHAR16 * DriverDisplayName = DefaultDriverDisplayName;
  CHAR16 * ControllerDisplayName = DefaultControllerDisplayName;
  CHAR16 * ChildDisplayName = DefaultChildDisplayName;

  // Wall of Shame:
  // Drivers of devices that improperly return GetControllerName and claim to be the controllers of anything you give them.
  // You can find some of these by searching for "ppControllerName" in EDK2.

  // In the comment adjacent each driver name you'll see listed the controller name that will ruin your day.
  #define NUM_ON_WALL 4

  CONST CHAR16 AmiPS2Drv[16] = L"AMI PS/2 Driver"; // L"Generic PS/2 Keyboard"
  CONST CHAR16 AsixUSBEth10Drv[34] = L"ASIX AX88772B Ethernet Driver 1.0"; // L"ASIX AX88772B USB Fast Ethernet Controller"
  CONST CHAR16 SocketLayerDrv[20] = L"Socket Layer Driver"; // L"Socket Layer";
  CONST CHAR16 Asix10100EthDrv[24] = L"AX88772 Ethernet Driver"; // L"AX88772 10/100 Ethernet"

  CONST CHAR16 * CONST Wall_of_Shame[NUM_ON_WALL] = {AmiPS2Drv, AsixUSBEth10Drv, SocketLayerDrv, Asix10100EthDrv};

  // End Wall_of_Shame init

  // We can pick which graphics output device we want (handy for multi-GPU setups)...
  EFI_HANDLE *GraphicsHandles; // Array of discovered graphics handles that support the Graphics Output Protocol
  GOPStatus = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &GraphicsOutputProtocol, NULL, &NumHandlesInHandleBuffer, &GraphicsHandles); // This automatically allocates pool for GraphicsHandles
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"GraphicsTable LocateHandleBuffer error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }

  Print(L"\r\n");
  if(NumHandlesInHandleBuffer == 1) // Grammar
  {
    Print(L"There is %llu UEFI graphics device:\r\n\n", NumHandlesInHandleBuffer);
  }
  else
  {
    Print(L"There are %llu UEFI graphics devices:\r\n\n", NumHandlesInHandleBuffer);
  }

  CHAR16 ** NameBuffer; // Pointer to a list of pointers that describe the string names of each output device. Each entry in the list is a pointer to CHAR16s, i.e. a string.
  GOPStatus = uefi_call_wrapper(BS->AllocatePool, 3, EfiBootServicesData, sizeof(CHAR16*) * NumHandlesInHandleBuffer, (void**)&NameBuffer); // Allocate space for the list
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"NameBuffer AllocatePool error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }

  // NOTE: The UEFI names of drivers are meaningless after ExitBootServices() is called, which is why a buffer of names is not a part of the GPU_Configs passed to the kernel.
  // The memory address of GraphicsHandles[DevNum] will also be meaningless at that stage, too, since it'll be somewhere in EfiBootServicesData.
  // The OS is just meant to have framebuffers left over from all this initialization, and names derived from ACPI or VEN:DEV ID lookups are to be used in an OS instead.

  // List all GPUs

  // Get all NAME2-supporting handles
  EFI_HANDLE *Name2Handles;

  GOPStatus = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &ComponentName2Protocol, NULL, &NumName2Handles, &Name2Handles); // This automatically allocates pool for Name2Handles
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"Name2Handles LocateHandleBuffer error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }
  EFI_HANDLE *DevPathHandles;

  GOPStatus = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &DevicePathProtocol, NULL, &NumDevPathHandles, &DevPathHandles); // This automatically allocates pool for DevPathHandles
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"DevPathHandles LocateHandleBuffer error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }

  for(DevNum = 0; DevNum < NumHandlesInHandleBuffer; DevNum++)
  {
    DriverDisplayName = DefaultDriverDisplayName;
    ControllerDisplayName = DefaultControllerDisplayName;
    ChildDisplayName = DefaultChildDisplayName;

    EFI_DEVICE_PATH *DevicePath_Graphics; // For GraphicsHandles, they'll always have a devpath because they use ACPI _ADR and they describe a physical output device. VMs are weird, though, and may have some extra virtualized things with GOP protocols.

    GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, GraphicsHandles[DevNum], &DevicePathProtocol, (void**)&DevicePath_Graphics, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL); // Need Device Path Node of GOP device
    if(GOPStatus == EFI_SUCCESS)
    {
      UINTN CntlrPathSize = DevicePathSize(DevicePath_Graphics) - DevicePathNodeLength(DevicePath_Graphics) + 4; // Add 4 bytes to account for the end node

      // Find the controller that corresponds to the GraphicsHandle's device path

      EFI_DEVICE_PATH *DevicePath_DevPath;
      UINT64 CntlrIndex = 0;

      for(CntlrIndex = 0; CntlrIndex < NumDevPathHandles; CntlrIndex++)
      {
        // Per https://github.com/tianocore/edk2/blob/master/ShellPkg/Library/UefiShellDriver1CommandsLib/DevTree.c
        // Controllers don't have DriverBinding or LoadedImage

        GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &DriverBindingProtocol, NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
        if (!EFI_ERROR (GOPStatus))
        {
          continue;
        }

        GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &LoadedImageProtocol, NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
        if (!EFI_ERROR (GOPStatus))
        {
          continue;
        }

        // The controllers we want don't have SimpFs, which seems to cause crashes if we don't skip them. Apparently if a FAT32 controller handle is paired with an NTFS driver, the system locks up.
        // This would only be a problem with the current method if the graphics output device also happened to be a user-writable storage device. I don't know how the UEFI GOP of a Radeon SSG is set up, and that's the only edge case I can think of.
        // If the SSD were writable, it would probably have a separate controller on a similar path to the GOP device, and better to just reject SimpFS tobe safe.
        GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &FileSystemProtocol, NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
        if (!EFI_ERROR (GOPStatus))
        {
          continue;
        }

        // Get controller's device path
        GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &DevicePathProtocol, (void**)&DevicePath_DevPath, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if(EFI_ERROR(GOPStatus))
        {
          Print(L"DevPathHandles OpenProtocol error. 0x%llx\r\n", GOPStatus);
          return GOPStatus;
        }

        // Match device paths; DevPath is a Multi, Graphics is a Single
        // This will fail on certain kinds of systems, like Hyper-V VMs. This method is made with PCI-Express graphics devices in mind.
        UINTN ThisCntlrPathSize = DevicePathSize(DevicePath_DevPath);

        if(ThisCntlrPathSize != CntlrPathSize)
        { // Might be something like PciRoot(0), which would match DevPath_Graphics for a PCI-E GPU without this check
          continue;
        }

        if(LibMatchDevicePaths(DevicePath_DevPath, DevicePath_Graphics))
        {
          // Found it. The desired controller is DevPathHandles[CntlrIndex]

          // Now match controller to its Name2-supporting driver
          for(UINT64 Name2DriverIndex = 0; Name2DriverIndex < NumName2Handles; Name2DriverIndex++)
          {

            // Check if Name2Handles[Name2DriverIndex] manages the DevPathHandles[CntlrIndex] controller
            // See EfiTestManagedDevice at
            // https://github.com/tianocore-docs/edk2-UefiDriverWritersGuide/blob/master/11_uefi_driver_and_controller_names/113_getcontrollername_implementations/1132_bus_drivers_and_hybrid_drivers.md
            // and the implementation at https://github.com/tianocore/edk2/blob/master/MdePkg/Library/UefiLib/UefiLib.c

            VOID * ManagedInterface; // Need a throwaway pointer for OpenProtocol BY_DRIVER

            GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &PciIoProtocol, &ManagedInterface, Name2Handles[Name2DriverIndex], DevPathHandles[CntlrIndex], EFI_OPEN_PROTOCOL_BY_DRIVER);
            if(!EFI_ERROR(GOPStatus))
            {
              GOPStatus = uefi_call_wrapper(BS->CloseProtocol, 4, DevPathHandles[CntlrIndex], &PciIoProtocol, Name2Handles[Name2DriverIndex], DevPathHandles[CntlrIndex]);
              if(EFI_ERROR(GOPStatus))
              {
                Print(L"DevPathHandles Name2Handles CloseProtocol error. 0x%llx\r\n", GOPStatus);
                return GOPStatus;
              }
              // No match!
              continue;
            }
            else if(GOPStatus != EFI_ALREADY_STARTED)
            {
              // No match!
              continue;
            }
            // Yes, found it! Get names.

            EFI_COMPONENT_NAME2_PROTOCOL *Name2Device;

            // Open Name2 Protocol
            GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, Name2Handles[Name2DriverIndex], &ComponentName2Protocol, (void**)&Name2Device, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            if(EFI_ERROR(GOPStatus))
            {
              Print(L"Name2Device OpenProtocol error. 0x%llx\r\n", GOPStatus);
              return GOPStatus;
            }

            // Get driver's name
            GOPStatus = uefi_call_wrapper(Name2Device->GetDriverName, 3, Name2Device, LanguageToUse, &DriverDisplayName);
            if(GOPStatus == EFI_UNSUPPORTED)
            {
              GOPStatus = uefi_call_wrapper(Name2Device->GetDriverName, 3, Name2Device, LanguageToUse2, &DriverDisplayName);
              if(GOPStatus == EFI_UNSUPPORTED)
              {
                GOPStatus = uefi_call_wrapper(Name2Device->GetDriverName, 3, Name2Device, LanguageToUse3, &DriverDisplayName);
              }
            }
            if(EFI_ERROR(GOPStatus))
            {
              // You know, we have specifications for a reason.
              // Those who refuse to follow them get this.
              DriverDisplayName = DefaultDriverDisplayName;
            }
            // Got driver's name

            // Get controller's name
            GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], NULL, LanguageToUse, &ControllerDisplayName); // The child should be NULL to get the controller's name.
            if(GOPStatus == EFI_UNSUPPORTED)
            {
              GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], NULL, LanguageToUse2, &ControllerDisplayName);
              if(GOPStatus == EFI_UNSUPPORTED)
              {
                GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], NULL, LanguageToUse3, &ControllerDisplayName);
              }
            }
            if(EFI_ERROR(GOPStatus))
            {
              // You know, we have specifications for a reason.
              // Those who refuse to follow them get this.
              ControllerDisplayName = DefaultControllerDisplayName;
            }
            // Got controller's name

            // Get child's name
            GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], GraphicsHandles[DevNum], LanguageToUse, &ChildDisplayName);
            if(GOPStatus == EFI_UNSUPPORTED)
            {
              GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], GraphicsHandles[DevNum], LanguageToUse2, &ChildDisplayName);
              if(GOPStatus == EFI_UNSUPPORTED)
              {
                GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], GraphicsHandles[DevNum], LanguageToUse3, &ChildDisplayName);
              }
            }
            if(EFI_ERROR(GOPStatus))
            {
              // You know, we have specifications for a reason.
              // Those who refuse to follow them get this.
              ChildDisplayName = DefaultChildDisplayName;
            }

            // Got child's name
            break;

          } // End for Name2DriverIndex
          break;
        } // End if match controller to GraphicsHandle's device path
      } // End for CntlrIndex

      // It's possible that a device is missing a child name, but has the controller and device names. The triple if() statement below covers this.

      // After all that, if still no name, it's probably a VM or something weird that doesn't implement ACPI ADR or PCIe.
      // This means there probably aren't devices like the PCIe root bridge to mess with device path matching.
      // So use a more generic method that's cognizant of this and doesn't enforce device path sizes.

      if((ControllerDisplayName == DefaultControllerDisplayName) && (DriverDisplayName == DefaultDriverDisplayName) && (ChildDisplayName == DefaultChildDisplayName))
      {
        // Find the controller that corresponds to the GraphicsHandle's device path

        // These have already been defined.
        // EFI_DEVICE_PATH *DevicePath_DevPath;
        // UINT64 CntlrIndex = 0;

        for(CntlrIndex = 0; CntlrIndex < NumDevPathHandles; CntlrIndex++)
        {
          // Per https://github.com/tianocore/edk2/blob/master/ShellPkg/Library/UefiShellDriver1CommandsLib/DevTree.c
          // Controllers don't have DriverBinding or LoadedImage

          GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &DriverBindingProtocol, NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
          if (!EFI_ERROR (GOPStatus))
          {
            continue;
          }

          GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &LoadedImageProtocol, NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
          if (!EFI_ERROR (GOPStatus))
          {
            continue;
          }

          // The controllers we want shouldn't have SimpFs, which seems to cause crashes if we don't skip them. Apparently if a FAT32 controller handle is paired with an NTFS driver, the system locks up.
          // This would only be a problem with the current method if the graphics output device also happened to be a user-writable storage device. I don't know how the UEFI GOP of a Radeon SSG is set up, and that's the only edge case I can think of.
          // If the SSD were writable, it would probably have a separate controller on a similar path to the GOP device, and better to just reject SimpFS tobe safe.
          // However, if this is in a VM, all bets are off and it's better to be safe than sorry.
          GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &FileSystemProtocol, NULL, NULL, NULL, EFI_OPEN_PROTOCOL_TEST_PROTOCOL);
          if (!EFI_ERROR (GOPStatus))
          {
            continue;
          }
          
          // Get controller's device path
          GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, DevPathHandles[CntlrIndex], &DevicePathProtocol, (void**)&DevicePath_DevPath, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
          if(EFI_ERROR(GOPStatus))
          {
            Print(L"Funky DevPathHandles OpenProtocol error. 0x%llx\r\n", GOPStatus);
            return GOPStatus;
          }

          // Match device paths; DevPath is a Multi, Graphics is a Single
          // DevPath could be as generic as VMBus or something, which is fine for this case since it's not PCIe.
          if(LibMatchDevicePaths(DevicePath_DevPath, DevicePath_Graphics))
          {
            // Found something on controller DevPathHandles[CntlrIndex]
            // Now match controller to its Name2-supporting driver
            for(UINT64 Name2DriverIndex = 0; Name2DriverIndex < NumName2Handles; Name2DriverIndex++)
            {
              // Check if Name2Handles[Name2DriverIndex] manages the DevPathHandles[CntlrIndex] controller
              // See EfiTestManagedDevice at
              // https://github.com/tianocore-docs/edk2-UefiDriverWritersGuide/blob/master/11_uefi_driver_and_controller_names/113_getcontrollername_implementations/1132_bus_drivers_and_hybrid_drivers.md
              // and the implementation at https://github.com/tianocore/edk2/blob/master/MdePkg/Library/UefiLib/UefiLib.c

              EFI_COMPONENT_NAME2_PROTOCOL *Name2Device;

              // Open Name2 Protocol
              GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, Name2Handles[Name2DriverIndex], &ComponentName2Protocol, (void**)&Name2Device, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
              if(EFI_ERROR(GOPStatus))
              {
                Print(L"Funky Name2Device OpenProtocol error. 0x%llx\r\n", GOPStatus);
                return GOPStatus;
              }

              // Get driver's name
              GOPStatus = uefi_call_wrapper(Name2Device->GetDriverName, 3, Name2Device, LanguageToUse, &DriverDisplayName);
              if(GOPStatus == EFI_UNSUPPORTED)
              {
                GOPStatus = uefi_call_wrapper(Name2Device->GetDriverName, 3, Name2Device, LanguageToUse2, &DriverDisplayName);
                if(GOPStatus == EFI_UNSUPPORTED)
                {
                  GOPStatus = uefi_call_wrapper(Name2Device->GetDriverName, 3, Name2Device, LanguageToUse3, &DriverDisplayName);
                }
              }
              if(EFI_ERROR(GOPStatus))
              {
                // You know, we have specifications for a reason.
                // Those who refuse to follow them get this.
                DriverDisplayName = DefaultDriverDisplayName;
              }
              else // Wall of Shame check
              {
                UINTN KnownBadDriversIter;
                UINTN a = StrSize(DriverDisplayName);

                for(KnownBadDriversIter = 0; KnownBadDriversIter < NUM_ON_WALL; KnownBadDriversIter++)
                {
                  UINTN b = StrSize(Wall_of_Shame[KnownBadDriversIter]);
                  if(Compare(DriverDisplayName, Wall_of_Shame[KnownBadDriversIter], (a < b) ? a : b)) // Need to compare data, not pointers
                  {
                    // Get MAD. I don't want your damn lemons! What am I supposed to do with these?!
                    // (Props to anyone who gets the reference)
                    DriverDisplayName = DefaultDriverDisplayName;
                    break;
                  }
                }
                if(KnownBadDriversIter < NUM_ON_WALL) // If it broke out of the loop...
                {
                  continue; // Try the next Name2DriverIndex
                }
                // Otherwise we might have found the device we're looking for. ...Hopefully it's not another thing to add to the Wall of Shame.
              }
              // Got driver's name

              // Get controller's name
              GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], NULL, LanguageToUse, &ControllerDisplayName); // The child should be NULL to get the controller's name.
              if(GOPStatus == EFI_UNSUPPORTED)
              {
                GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], NULL, LanguageToUse2, &ControllerDisplayName);
                if(GOPStatus == EFI_UNSUPPORTED)
                {
                  GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], NULL, LanguageToUse3, &ControllerDisplayName);
                }
              }
              if(EFI_ERROR(GOPStatus))
              {
                // You know, we have specifications for a reason.
                // Those who refuse to follow them get this.
                ControllerDisplayName = DefaultControllerDisplayName;
              }
              // Got controller's name
              // Get child's name
              GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], GraphicsHandles[DevNum], LanguageToUse, &ChildDisplayName);
              if(GOPStatus == EFI_UNSUPPORTED)
              {
                GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], GraphicsHandles[DevNum], LanguageToUse2, &ChildDisplayName);
                if(GOPStatus == EFI_UNSUPPORTED)
                {
                  GOPStatus = uefi_call_wrapper(Name2Device->GetControllerName, 5, Name2Device, DevPathHandles[CntlrIndex], GraphicsHandles[DevNum], LanguageToUse3, &ChildDisplayName);
                }
              }
              if(EFI_ERROR(GOPStatus))
              {
                // You know, we have specifications for a reason.
                // Those who refuse to follow them get this.
                ChildDisplayName = DefaultChildDisplayName;
              }

              // Got child's name
              // Hopefully this wasn't another case of the "PS/2 driver that tries to claim that all handles are its children" :P
              // There's no way to check without explicitly blacklisting by driver name or filtering by protocol (as was done above).
              // I think Hyper-V's video driver does something similar, though it reports Hyper-V Video Controller as the child of Hyper-V Video Controller,
              // which, in this specific case, is actually OK. Would've been nice if they'd implemented a proper ChildHandle, though.
              if(ChildDisplayName != DefaultChildDisplayName)
              {
                break;
                // There are too many things that have both driver and controller names match without a child name via this method, so this is the safest option.
              }
              // Nope, try another Name2 driver

            } // End for Name2DriverIndex
            if(ChildDisplayName != DefaultChildDisplayName)
            {
              break;
            }
            // Nope, try and see if another controller matches
            // If nothing matches, too bad: it'll just say "No Driver Name," "No Controller Name," and No Child Name" in whatever the below print order is.
          } // End if match controller to GraphicsHandle's device path
        } // End for CntlrIndex
      } // End funky graphics types method


      POOL_PRINT StringName = {0};
      CatPrint(&StringName, L"%c. %s: %s @ Memory Address 0x%llx, using %s\r\n", DevNum + 0x30, ControllerDisplayName, ChildDisplayName, GraphicsHandles[DevNum], DriverDisplayName); // CatPrint allocates pool
      NameBuffer[DevNum] = StringName.str;

    }
    else if(GOPStatus == EFI_UNSUPPORTED) // Need to do this because VMs can throw curveballs sometimes
    {
      POOL_PRINT StringName = {0};
      CatPrint(&StringName, L"%c. Weird unknown device @ Memory Address 0x%llx (is this in a VM?)\r\n", DevNum + 0x30, GraphicsHandles[DevNum]); // CatPrint allocates pool
      NameBuffer[DevNum] = StringName.str;


    }
    else if(EFI_ERROR(GOPStatus))
    {
      Print(L"GraphicsHandles DevicePath_Graphics OpenProtocol error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }
  } // End for(GraphicsHandles[DevNum]...)


  // Done with this massive array of DevPathHandles
  GOPStatus = uefi_call_wrapper(BS->FreePool, 1, DevPathHandles);
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"DevPathHandles FreePool error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }
/*
  // Done with massive array of DriverHandles
  GOPStatus = BS->FreePool(DriverHandles);
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"DriverHandles FreePool error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }
*/
  // Done with this massive array of Name2Handles
  GOPStatus = uefi_call_wrapper(BS->FreePool, 1, Name2Handles);
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"Name2Handles FreePool error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }

  // If applicable, select a GPU. Otherwise, skip all the way to single GPU configuration.
  if(NumHandlesInHandleBuffer > 1)
  {
    // Using this as the choice holder
    // This sets the default option.
    DevNum = 2;
    UINT64 timeout_seconds = GPU_MENU_TIMEOUT_SECONDS;
    // FYI: The EFI watchdog has something like a 5 minute timeout before it resets the system if ExitBootServices() hasn't been reached.
    // Not all systems have a watchdog enabled, but enough do that knowing about the watchdog (and assuming there's always one) is useful.

#ifdef DEBUG_PIOUS

    // User selection
    while(0x30 > Key.UnicodeChar || Key.UnicodeChar > 0x33)
    {
      for(UINTN DevNumIter = 0; DevNumIter < NumHandlesInHandleBuffer; DevNumIter++)
      {
        Print(L"%s", NameBuffer[DevNumIter]);
      }
      Print(L"\r\n");

      Print(L"Configure all graphics devices or just one?\r\n");
      Print(L"0. Configure all individually\r\n");
      Print(L"1. Configure one\r\n");
      Print(L"2. Configure all to use default resolutions of active displays (usually native)\r\n");
      Print(L"3. Configure all to use 1024x768\r\n");
      Print(L"\r\nNote: The \"active display(s)\" on a GPU are determined by the GPU's firmware, and not all output ports may be currently active.\r\n\n");

      while(timeout_seconds)
      {
        Print(L"Please select an option. Defaulting to option %llu in %llu... \r", DevNum, timeout_seconds);
        GOPStatus = WaitForSingleEvent(ST->ConIn->WaitForKey, 10000000); // Timeout units are 100ns
        if(GOPStatus != EFI_TIMEOUT)
        {
          GOPStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
          if (EFI_ERROR(GOPStatus))
          {
            Print(L"\nError reading keystroke. 0x%llx\r\n", GOPStatus);
            return GOPStatus;
          }

          Print(L"\n\nOption %c selected.\r\n\n", Key.UnicodeChar);
          break;
        }
        timeout_seconds -= 1;
      }

      if(!timeout_seconds)
      {
        Print(L"\n\nDefaulting to option %llu...\r\n\n", DevNum);
        break;
      }
    }

    if(timeout_seconds) // Only update DevNum if the loop ended due to a keypress. The loop won't have exited with time remaining without a valid key pressed.
    {
      DevNum = (UINT64)(Key.UnicodeChar - 0x30); // Convert user input character from unicode to number
    }

    Key.UnicodeChar = 0; // Reset input
    GOPStatus = uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE); // Reset input buffer
    if (EFI_ERROR(GOPStatus))
    {
      Print(L"Error resetting input buffer. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }
#endif
  }

  if((NumHandlesInHandleBuffer > 1) && (DevNum == 0))
  {
    // Configure all individually
    // NOTE: If there's only 1 available mode for a given device, this will just auto-set its output to that; no need for explicit choice there.

    // Setup
    Graphics->NumberOfFrameBuffers = NumHandlesInHandleBuffer;
    GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, Graphics->NumberOfFrameBuffers*sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE), (void**)&Graphics->GPUArray);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GPUArray AllocatePool error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    // Configure
    for(DevNum = 0; DevNum < NumHandlesInHandleBuffer; DevNum++)
    {

      EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPTable;

      // Mode->Info gets reserved once SizeOfInfo is determined (via QueryMode()).

      GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, GraphicsHandles[DevNum], &GraphicsOutputProtocol, (void**)&GOPTable, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GraphicsTable OpenProtocol error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      if(GOPTable->Mode->MaxMode == 1) // Grammar
      {
        mode = 0; // If there's only one mode, it's going to be mode 0.
      }
      else
      {
        // Get supported graphics modes
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GOPInfo2; // Querymode allocates GOPInfo
        while(0x30 > Key.UnicodeChar || Key.UnicodeChar > (0x30 + GOPTable->Mode->MaxMode - 1))
        {
          Print(L"%s", NameBuffer[DevNum]);
          Print(L"\r\n");

          Print(L"%u available graphics modes found.\r\n\n", GOPTable->Mode->MaxMode);

          Print(L"Current Mode: %c\r\n", GOPTable->Mode->Mode + 0x30);
          for(mode = 0; mode < GOPTable->Mode->MaxMode; mode++) // Valid modes are from 0 to MaxMode - 1
          {
            GOPStatus = uefi_call_wrapper(GOPTable->QueryMode, 4, GOPTable, mode, &GOPInfoSize, &GOPInfo2); // IN IN OUT OUT
            if(EFI_ERROR(GOPStatus))
            {
              Print(L"GraphicsTable QueryMode error. 0x%llx\r\n", GOPStatus);
              return GOPStatus;
            }
            Print(L"%c. %ux%u, PxPerScanLine: %u, PxFormat: %s\r\n", mode + 0x30, GOPInfo2->HorizontalResolution, GOPInfo2->VerticalResolution, GOPInfo2->PixelsPerScanLine, PxFormats[GOPInfo2->PixelFormat]);

            // Don't need GOPInfo2 anymore
            GOPStatus = uefi_call_wrapper(BS->FreePool, 1, GOPInfo2);
            if(EFI_ERROR(GOPStatus))
            {
              Print(L"Error freeing GOPInfo2 pool. 0x%llx\r\n", GOPStatus);
              return GOPStatus;
            }
          }

          Print(L"\r\nPlease select a graphics mode. (0 - %c)\r\n", 0x30 + GOPTable->Mode->MaxMode - 1);

          while ((GOPStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key)) == EFI_NOT_READY);

          Print(L"\r\nSelected graphics mode %c.\r\n\n", Key.UnicodeChar);
        }
        mode = (UINT32)(Key.UnicodeChar - 0x30);
        Key.UnicodeChar = 0;

        Print(L"Setting graphics mode %u of %u.\r\n\n", mode + 1, GOPTable->Mode->MaxMode);
      }

      // Set mode
      // This is supposed to black the screen out per spec, but apparently not every GPU got the memo.
      GOPStatus = uefi_call_wrapper(GOPTable->SetMode, 2, GOPTable, mode);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GraphicsTable SetMode error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      Print(L"FrameBufferBase: 0x%016llx, FrameBufferSize: 0x%llx\r\n", GOPTable->Mode->FrameBufferBase, GOPTable->Mode->FrameBufferSize); // Per spec, the FrameBufferBase might be 0 until SetMode is called

      // Allocate graphics mode info
      GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, GOPTable->Mode->SizeOfInfo, (void**)&Graphics->GPUArray[DevNum].Info);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GOP Mode->Info AllocatePool error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      // Store graphics mode info
      // Can't blanketly store Mode struct because Mode->Info pointer in array will get overwritten
      Graphics->GPUArray[DevNum].MaxMode = GOPTable->Mode->MaxMode;
      Graphics->GPUArray[DevNum].Mode = GOPTable->Mode->Mode;
      Graphics->GPUArray[DevNum].SizeOfInfo = GOPTable->Mode->SizeOfInfo;
      Graphics->GPUArray[DevNum].FrameBufferBase = GOPTable->Mode->FrameBufferBase;
      Graphics->GPUArray[DevNum].FrameBufferSize = GOPTable->Mode->FrameBufferSize;
      // Can blanketly override Info struct, though (no pointers in it, just raw data)
      *(Graphics->GPUArray[DevNum].Info) = *(GOPTable->Mode->Info);

    } // End for each individual DevNum
  }
  else if((NumHandlesInHandleBuffer > 1) && (DevNum == 1))
  {
    // Configure one
    // NOTE: If there's only 1 available mode for a given device, this will just auto-set its output to that; no need for explicit choice there.

    // Setup
    Graphics->NumberOfFrameBuffers = 1;
    GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, Graphics->NumberOfFrameBuffers*sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE), (void**)&Graphics->GPUArray);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GPUArray AllocatePool error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    // Configure

    // User selection of GOP-supporting device
    while(0x30 > Key.UnicodeChar || Key.UnicodeChar > (0x30 + NumHandlesInHandleBuffer - 1))
    {
      for(UINTN DevNumIter = 0; DevNumIter < NumHandlesInHandleBuffer; DevNumIter++)
      {
//        Print(L"%c. GPU #%c: 0x%llx\r\n", DevNum + 0x30, DevNum + 0x30, GraphicsHandles[DevNum]); // Memory address of GPU handle
        Print(L"%s", NameBuffer[DevNumIter]);
      }
      Print(L"\r\n");
      Print(L"Please select an output device. (0 - %llu)\r\n", NumHandlesInHandleBuffer - 1);

      while ((GOPStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key)) == EFI_NOT_READY);

      Print(L"\r\nDevice %c selected.\r\n\n", Key.UnicodeChar);
    }
    DevNum = (UINT64)(Key.UnicodeChar - 0x30); // Convert user input character from UTF-16 to number
    Key.UnicodeChar = 0; // Reset input

    EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPTable;

    GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, GraphicsHandles[DevNum], &GraphicsOutputProtocol, (void**)&GOPTable, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GraphicsTable OpenProtocol error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    if(GOPTable->Mode->MaxMode == 1) // Grammar
    {
      mode = 0; // If there's only one mode, it's going to be mode 0.
    }
    else
    {
      // Get supported graphics modes
      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GOPInfo2; // Querymode allocates GOPInfo
      while(0x30 > Key.UnicodeChar || Key.UnicodeChar > (0x30 + GOPTable->Mode->MaxMode - 1))
      {
        Print(L"%s", NameBuffer[DevNum]);
        Print(L"\r\n");

        Print(L"%u available graphics modes found.\r\n\n", GOPTable->Mode->MaxMode);

        Print(L"Current Mode: %c\r\n", GOPTable->Mode->Mode + 0x30);
        for(mode = 0; mode < GOPTable->Mode->MaxMode; mode++) // Valid modes are from 0 to MaxMode - 1
        {
          GOPStatus = uefi_call_wrapper(GOPTable->QueryMode, 4, GOPTable, mode, &GOPInfoSize, &GOPInfo2); // IN IN OUT OUT
          if(EFI_ERROR(GOPStatus))
          {
            Print(L"GraphicsTable QueryMode error. 0x%llx\r\n", GOPStatus);
            return GOPStatus;
          }
          Print(L"%c. %ux%u, PxPerScanLine: %u, PxFormat: %s\r\n", mode + 0x30, GOPInfo2->HorizontalResolution, GOPInfo2->VerticalResolution, GOPInfo2->PixelsPerScanLine, PxFormats[GOPInfo2->PixelFormat]);

          // Don't need GOPInfo2 anymore
          GOPStatus = uefi_call_wrapper(BS->FreePool, 1, GOPInfo2);
          if(EFI_ERROR(GOPStatus))
          {
            Print(L"Error freeing GOPInfo2 pool. 0x%llx\r\n", GOPStatus);
            return GOPStatus;
          }
        }

        Print(L"\r\nPlease select a graphics mode. (0 - %c)\r\n", 0x30 + GOPTable->Mode->MaxMode - 1);

        while ((GOPStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key)) == EFI_NOT_READY);

        Print(L"\r\nSelected graphics mode %c.\r\n\n", Key.UnicodeChar);
      }
      mode = (UINT32)(Key.UnicodeChar - 0x30);
      Key.UnicodeChar = 0;

      Print(L"Setting graphics mode %u of %u.\r\n\n", mode + 1, GOPTable->Mode->MaxMode);
    }

    // Set mode
    // This is supposed to black the screen out per spec, but apparently not every GPU got the memo.
    GOPStatus = uefi_call_wrapper(GOPTable->SetMode, 2, GOPTable, mode);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GraphicsTable SetMode error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    DevNum = 0; // There's only one item in the array
    // Allocate graphics mode info
    GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, GOPTable->Mode->SizeOfInfo, (void**)&Graphics->GPUArray[DevNum].Info);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GOP Mode->Info AllocatePool error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    // Store graphics mode info
    // Can't blanketly store Mode struct because Mode->Info pointer in array will get overwritten
    Graphics->GPUArray[DevNum].MaxMode = GOPTable->Mode->MaxMode;
    Graphics->GPUArray[DevNum].Mode = GOPTable->Mode->Mode;
    Graphics->GPUArray[DevNum].SizeOfInfo = GOPTable->Mode->SizeOfInfo;
    Graphics->GPUArray[DevNum].FrameBufferBase = GOPTable->Mode->FrameBufferBase;
    Graphics->GPUArray[DevNum].FrameBufferSize = GOPTable->Mode->FrameBufferSize;
    // Can blanketly override Info struct, though (no pointers in it, just raw data)
    *(Graphics->GPUArray[DevNum].Info) = *(GOPTable->Mode->Info);

  // End configure one only
  }
  else if((NumHandlesInHandleBuffer > 1) && (DevNum == 2))
  {
    // Configure each device to use the default resolutions of each connected display (usually native)

    // Setup
    Graphics->NumberOfFrameBuffers = NumHandlesInHandleBuffer;
    GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, Graphics->NumberOfFrameBuffers*sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE), (void**)&Graphics->GPUArray);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GPUArray AllocatePool error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    // Configure
    for(DevNum = 0; DevNum < NumHandlesInHandleBuffer; DevNum++)
    {

      EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPTable;

      GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, GraphicsHandles[DevNum], &GraphicsOutputProtocol, (void**)&GOPTable, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GraphicsTable OpenProtocol error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      // Set mode 0
      mode = 0;

      Print(L"Setting graphics mode %u of %u.\r\n\n", mode + 1, GOPTable->Mode->MaxMode);

      // Set mode
      // This is supposed to black the screen out per spec, but apparently not every GPU got the memo.
      GOPStatus = uefi_call_wrapper(GOPTable->SetMode, 2, GOPTable, mode);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GraphicsTable SetMode error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      // Allocate graphics mode info
      GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, GOPTable->Mode->SizeOfInfo, (void**)&Graphics->GPUArray[DevNum].Info);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GOP Mode->Info AllocatePool error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      // Store graphics mode info
      // Can't blanketly store Mode struct because Mode->Info pointer in array will get overwritten
      Graphics->GPUArray[DevNum].MaxMode = GOPTable->Mode->MaxMode;
      Graphics->GPUArray[DevNum].Mode = GOPTable->Mode->Mode;
      Graphics->GPUArray[DevNum].SizeOfInfo = GOPTable->Mode->SizeOfInfo;
      Graphics->GPUArray[DevNum].FrameBufferBase = GOPTable->Mode->FrameBufferBase;
      Graphics->GPUArray[DevNum].FrameBufferSize = GOPTable->Mode->FrameBufferSize;
      // Can blanketly override Info struct, though (no pointers in it, just raw data)
      *(Graphics->GPUArray[DevNum].Info) = *(GOPTable->Mode->Info);


    } // End for each individual DevNum

    // End default res for each
  }
  else if((NumHandlesInHandleBuffer > 1) && (DevNum == 3))
  {
    // Configure all to use 1024x768
    // Despite the UEFI spec's mandating only 640x480 and 800x600, everyone who supports Windows must also support 1024x768

    // Setup
    Graphics->NumberOfFrameBuffers = NumHandlesInHandleBuffer;
    GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, Graphics->NumberOfFrameBuffers*sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE), (void**)&Graphics->GPUArray);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GPUArray AllocatePool error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    // Configure
    for(DevNum = 0; DevNum < NumHandlesInHandleBuffer; DevNum++)
    {

      EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPTable;

      GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6, GraphicsHandles[DevNum], &GraphicsOutputProtocol, (void**)&GOPTable, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GraphicsTable OpenProtocol error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      // Get supported graphics modes
      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GOPInfo2; // Querymode allocates GOPInfo
      for(mode = 0; mode < GOPTable->Mode->MaxMode; mode++) // Valid modes are from 0 to MaxMode - 1
      {
        GOPStatus = uefi_call_wrapper(GOPTable->QueryMode, 4, GOPTable, mode, &GOPInfoSize, &GOPInfo2); // IN IN OUT OUT
        if(EFI_ERROR(GOPStatus))
        {
          Print(L"GraphicsTable QueryMode error. 0x%llx\r\n", GOPStatus);
          return GOPStatus;
        }
        if((GOPInfo2->HorizontalResolution == 1024) && (GOPInfo2->VerticalResolution == 768))
        {
          // Don't need GOPInfo2 anymore
          GOPStatus = uefi_call_wrapper(BS->FreePool, 1, GOPInfo2);
          if(EFI_ERROR(GOPStatus))
          {
            Print(L"Error freeing GOPInfo2 pool. 0x%llx\r\n", GOPStatus);
            return GOPStatus;
          }
          break; // Use this mode
        }

        // Don't need GOPInfo2 anymore
        GOPStatus = uefi_call_wrapper(BS->FreePool, 1, GOPInfo2);
        if(EFI_ERROR(GOPStatus))
        {
          Print(L"Error freeing GOPInfo2 pool. 0x%llx\r\n", GOPStatus);
          return GOPStatus;
        }
      }
      if(mode == GOPTable->Mode->MaxMode) // Hyper-V only has a 1024x768 mode, and it's mode 0
      {
        Print(L"Odd. No 1024x768 mode found. Using mode 0...\r\n");
        mode = 0;
      }

      Print(L"Setting graphics mode %u of %u.\r\n\n", mode + 1, GOPTable->Mode->MaxMode);

      // Set mode
      // This is supposed to black the screen out per spec, but apparently not every GPU got the memo.
      GOPStatus = uefi_call_wrapper(GOPTable->SetMode, 2, GOPTable, mode);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GraphicsTable SetMode error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      // Allocate graphics mode info
      GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, GOPTable->Mode->SizeOfInfo, (void**)&Graphics->GPUArray[DevNum].Info);
      if(EFI_ERROR(GOPStatus))
      {
        Print(L"GOP Mode->Info AllocatePool error. 0x%llx\r\n", GOPStatus);
        return GOPStatus;
      }

      // Store graphics mode info
      // Can't blanketly store Mode struct because Mode->Info pointer in array will get overwritten
      Graphics->GPUArray[DevNum].MaxMode = GOPTable->Mode->MaxMode;
      Graphics->GPUArray[DevNum].Mode = GOPTable->Mode->Mode;
      Graphics->GPUArray[DevNum].SizeOfInfo = GOPTable->Mode->SizeOfInfo;
      Graphics->GPUArray[DevNum].FrameBufferBase = GOPTable->Mode->FrameBufferBase;
      Graphics->GPUArray[DevNum].FrameBufferSize = GOPTable->Mode->FrameBufferSize;
      // Can blanketly override Info struct, though (no pointers in it, just raw data)
      *(Graphics->GPUArray[DevNum].Info) = *(GOPTable->Mode->Info);

    } // End for each individual DevNum

    // End 1024x768
  }
  else
  {
    // Single GPU
    // NOTE: If there's only 1 available mode for a given device, this will just auto-set its output to that; no need for explicit choice there.
    // Similarly, this single GPU case is the only one that has a timeout on its resolution menu. Muti-GPU options 0 and 1 assume the user is present to use them--there's no other way to get to them! (Multi-GPU options 2 and 3 are automatic modes, so they don't have menus.)

    // Setup
    Graphics->NumberOfFrameBuffers = 1;
    GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, Graphics->NumberOfFrameBuffers*sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE), (void**)&Graphics->GPUArray);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GPUArray AllocatePool error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    // Configure
    // Only one device
    DevNum = 0;

    EFI_GRAPHICS_OUTPUT_PROTOCOL *GOPTable;

    GOPStatus = uefi_call_wrapper(BS->OpenProtocol, 6,GraphicsHandles[DevNum], &GraphicsOutputProtocol, (void**)&GOPTable, ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GraphicsTable OpenProtocol error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    if(GOPTable->Mode->MaxMode == 1) // Grammar
    {
      mode = 0; // If there's only one mode, it's going to be mode 0.
    }
    else
    {
      // Default mode
      UINT32 default_mode = 0;
      UINT64 timeout_seconds = GPU_MENU_TIMEOUT_SECONDS;

      // Get supported graphics modes
      EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GOPInfo2; // Querymode allocates GOPInfo
      while(0x30 > Key.UnicodeChar || Key.UnicodeChar > (0x30 + GOPTable->Mode->MaxMode - 1))
      {
        Print(L"%s", NameBuffer[DevNum]);
        Print(L"\r\n");

        Print(L"%u available graphics modes found.\r\n\n", GOPTable->Mode->MaxMode);

        Print(L"Current Mode: %c\r\n", GOPTable->Mode->Mode + 0x30);
        for(mode = 0; mode < GOPTable->Mode->MaxMode; mode++) // Valid modes are from 0 to MaxMode - 1
        {
          GOPStatus = uefi_call_wrapper(GOPTable->QueryMode, 4, GOPTable, mode, &GOPInfoSize, &GOPInfo2); // IN IN OUT OUT
          if(EFI_ERROR(GOPStatus))
          {
            Print(L"GraphicsTable QueryMode error. 0x%llx\r\n", GOPStatus);
            return GOPStatus;
          }
          Print(L"%c. %ux%u, PxPerScanLine: %u, PxFormat: %s\r\n", mode + 0x30, GOPInfo2->HorizontalResolution, GOPInfo2->VerticalResolution, GOPInfo2->PixelsPerScanLine, PxFormats[GOPInfo2->PixelFormat]);

          // Don't need GOPInfo2 anymore
          GOPStatus = uefi_call_wrapper(BS->FreePool, 1, GOPInfo2);
          if(EFI_ERROR(GOPStatus))
          {
            Print(L"Error freeing GOPInfo2 pool. 0x%llx\r\n", GOPStatus);
            return GOPStatus;
          }
        }
        Print(L"\r\n");

        while(timeout_seconds)
        {
          Print(L"Please select a graphics mode. (0 - %c). Defaulting to mode %c in %llu... \r", 0x30 + GOPTable->Mode->MaxMode - 1, default_mode + 0x30, timeout_seconds);
          GOPStatus = WaitForSingleEvent(ST->ConIn->WaitForKey, 10000000); // Timeout units are 100ns
          if(GOPStatus != EFI_TIMEOUT)
          {
            GOPStatus = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
            if (EFI_ERROR(GOPStatus))
            {
              Print(L"\nError reading keystroke. 0x%llx\r\n", GOPStatus);
              return GOPStatus;
            }

            Print(L"\n\nSelected graphics mode %c.\r\n\n", Key.UnicodeChar);
            break;
          }
          timeout_seconds -= 1;
        }

        if(!timeout_seconds)
        {
          Print(L"\n\nDefaulting to mode %c...\r\n\n", default_mode + 0x30);
          mode = default_mode;
          break;
        }
      }

      if(timeout_seconds) // Only update mode if the loop ended due to a keypress. The loop won't have exited with time remaining without a valid key pressed.
      {
        mode = (UINT32)(Key.UnicodeChar - 0x30);
      }
      Key.UnicodeChar = 0;

      Print(L"Setting graphics mode %u of %u.\r\n\n", mode + 1, GOPTable->Mode->MaxMode);
    }

    // Set mode
    // This is supposed to black the screen out per spec, but apparently not every GPU got the memo.
    GOPStatus = uefi_call_wrapper(GOPTable->SetMode, 2, GOPTable, mode);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GraphicsTable SetMode error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }

    // Allocate graphics mode info
    GOPStatus = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, GOPTable->Mode->SizeOfInfo, (void**)&Graphics->GPUArray[DevNum].Info);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"GOP Mode->Info AllocatePool error. 0x%llx\r\n", GOPStatus);
      return GOPStatus;
    }
    // Store graphics mode info
    // Can't blanketly store Mode struct because Mode->Info pointer in array will get overwritten
    Graphics->GPUArray[DevNum].MaxMode = GOPTable->Mode->MaxMode;
    Graphics->GPUArray[DevNum].Mode = GOPTable->Mode->Mode;
    Graphics->GPUArray[DevNum].SizeOfInfo = GOPTable->Mode->SizeOfInfo;
    Graphics->GPUArray[DevNum].FrameBufferBase = GOPTable->Mode->FrameBufferBase;
    Graphics->GPUArray[DevNum].FrameBufferSize = GOPTable->Mode->FrameBufferSize;
    // Can blanketly override Info struct, though (no pointers in it, just raw data)
    *(Graphics->GPUArray[DevNum].Info) = *(GOPTable->Mode->Info);

  // End single GPU
  }

  // Don't need string names anymore
  for(UINTN StringNameFree = 0; StringNameFree < NumHandlesInHandleBuffer; StringNameFree++)
  {
    GOPStatus = uefi_call_wrapper(BS->FreePool, 1, NameBuffer[StringNameFree]);
    if(EFI_ERROR(GOPStatus))
    {
      Print(L"NameBuffer[%llu] FreePool error. 0x%llx\r\n", StringNameFree, GOPStatus);
      return GOPStatus;
    }
  }

  // Don't need NameBuffer anymore
  GOPStatus = uefi_call_wrapper(BS->FreePool, 1, NameBuffer);
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"NameBuffer FreePool error. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }

  // Don't need GraphicsHandles anymore
  GOPStatus = uefi_call_wrapper(BS->FreePool, 1, GraphicsHandles);
  if(EFI_ERROR(GOPStatus))
  {
    Print(L"Error freeing GraphicsHandles pool. 0x%llx\r\n", GOPStatus);
    return GOPStatus;
  }


  // Do a check to make sure the GPU driver supports direct framebuffer access. If not, the kernel will not boot.
  if((void*)(Graphics->GPUArray[0].FrameBufferBase) == NULL || Graphics->GPUArray[0].FrameBufferSize == 0)
  {
    Print(L"Framebuffer access is not supported. Consider upgrading your firmware.\r\n");
    return EFI_UNSUPPORTED;
  }



  return GOPStatus;
}




EFI_STATUS MapVirtualPages(UINTN physical, UINTN virt, UINTN pages, UINT32 flags, EFI_SYSTEM_TABLE * ST)
{
#ifdef x86_64

  Print(L"Mapping %u pages from 0x%llX to 0x%llX\r\n", pages, physical, virt);
	UINTN *pml4;

  asm volatile("mov %%cr3, %[dest]"
      : [dest] "=r" (pml4) // Outputs
      : // Inputs
      : // Clobbers
  );


	UINTN pml4i = (virt >> 39) & 0x1FF;
	UINTN pml3i = (virt >> 30) & 0x1FF;
	UINTN pml2i = (virt >> 21) & 0x1FF;
	UINTN pml1i = (virt >> 12) & 0x1FF;

	UINT64 *pml3 = NULL;
	UINT64 *pml2 = NULL;
	UINT64 *pml1 = NULL;

	if(!pml4[pml4i]) {
		uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1, (UINTN *) &pml3);
		ZeroMem(pml3, 0x1000);

		pml4[pml4i] = (UINTN) pml3 | 0x01 | 0x02;
	} else {
		pml3 = (UINTN *) (pml4[pml4i] & ~0xFFFUL);
	}

	if(!pml3[pml3i]) {
		uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1, (UINTN *) &pml2);
		ZeroMem(pml2, 0x1000);

		pml3[pml3i] = (UINTN) pml2 | 0x01 | 0x02;
	} else {
		pml2 = (UINTN *) (pml3[pml3i] & ~0xFFFUL);
	}

	if(!pml2[pml2i]) {
		uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, 1, (UINTN *) &pml1);
		ZeroMem(pml1, 0x1000);

		pml2[pml2i] = (UINTN) pml1 | 0x01 | 0x02;
	} else {
		pml1 = (UINTN *) (pml2[pml2i] & ~0xFFFUL);
	}

	for(UINTN i = 0; i < pages; i++) {
		pml1[pml1i + i] = (physical + (i << 12)) | flags;
		asm volatile ("invlpg (%0)" : : "r" (virt + (i << 12)) : "memory");

		if(pml1i + i > 511) {
			MapVirtualPages(physical + (i << 12), virt + (i << 12), pages - i, flags, ST);
		}
	}
#endif

	return EFI_SUCCESS;
}