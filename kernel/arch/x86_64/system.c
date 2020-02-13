#include "system.h"
#include "kernel/graphics.h"
#include "kernel/memory.h"
#include "ISR.h"


__attribute__((aligned(64))) uint64_t MinimalGDT[5] = {0, 0x00af9a000000ffff, 0x00cf92000000ffff, 0x0080890000000067, 0};
/*
  // Null
  ((uint64_t*)MinimalGDT)[0] = 0;

  // x86-64 Code (64-bit code segment)
  MinimalGDT[1].SegmentLimit1 = 0xffff;
  MinimalGDT[1].BaseAddress1 = 0;
  MinimalGDT[1].BaseAddress2 = 0;
  MinimalGDT[1].Misc1 = 0x9a; // P=1, DPL=0, S=1, Exec/Read
  MinimalGDT[1].SegmentLimit2andMisc2 = 0xaf; // G=1, D=0, L=1, AVL=0
  MinimalGDT[1].BaseAddress3 = 0;
  // Note the 'L' bit is specifically for x86-64 code segments, not data segments
  // Data segments need the 32-bit treatment instead

  // x86-64 Data (64- & 32-bit data segment)
  MinimalGDT[2].SegmentLimit1 = 0xffff;
  MinimalGDT[2].BaseAddress1 = 0;
  MinimalGDT[2].BaseAddress2 = 0;
  MinimalGDT[2].Misc1 = 0x92; // P=1, DPL=0, S=1, Read/Write
  MinimalGDT[2].SegmentLimit2andMisc2 = 0xcf; // G=1, D=1, L=0, AVL=0
  MinimalGDT[2].BaseAddress3 = 0;

  // Task Segment Entry (64-bit needs one, even though task-switching isn't supported)
  MinimalGDT[3].SegmentLimit1 = 0x67; // TSS struct is 104 bytes, so limit is 103 (0x67)
  MinimalGDT[3].BaseAddress1 = tss64_addr;
  MinimalGDT[3].BaseAddress2 = tss64_addr >> 16;
  MinimalGDT[3].Misc1 = 0x89; // P=1, DPL=0, S=0, TSS Type
  MinimalGDT[3].SegmentLimit2andMisc2 = 0x80; // G=1, D=0, L=0, AVL=0
  MinimalGDT[3].BaseAddress3 = tss64_addr >> 24;

  ((uint64_t*)MinimalGDT)[4] = tss64_addr >> 32; // TSS is a double-sized entry
*/

__attribute__((aligned(4096))) static uint64_t outermost_table[512] = {0};


void Initialize_System(LOADER_PARAMS* Parameters)
{
    Initialize_Memory(Parameters->Memory_Map_Size, Parameters->Memory_Map_Size, Parameters->Memory_Map, Parameters->Memory_Map_Descriptor_Version);
    Initialize_Display(Parameters->GPU_Configs->GPUArray[0]);

    Initialize_ISR();

    uint64_t reg;
    uint64_t reg2;


    //This disables paging setup by the UEFI firmware. This will give the OS the ability to write its own memory manager
    asm volatile("mov %%cr0, %[dest]"
        : [dest] "=r" (reg) // Outputs
        : // Inputs
        : // Clobbers
    );

    //Turn off the paging bit if it's on from UEFI
    if(reg & (1 << 7))
    {
        reg ^= (1 << 7);
        asm volatile("mov %[dest], %%cr0"
            : // Outputs
            : [dest] "r" (reg) // Inputs
            : // Clobbers
        );
    }

    uint64_t maxRAM = GetMaxMappedPhysicalAddress();
    uint64_t reg3;

    asm volatile("cpuid"
       : "=d" (reg) // Outputs
       : "a" (0x80000001) // Inputs (The value to put into %rax)
       : "%rbx", "%rcx"// Clobbers
    );

    if(reg & (1 << 26))
    {
        //Use 1GB Paging

        asm volatile("mov %%cr4, %[dest]"
            : [dest] "=r" (reg3) // Outputs
            : // Inputs
            : // Clobbers
        );

        if(maxRAM > (1ULL << 57)) //128 PB
        {
            //WAY too much RAM
            //TODO: Error out
        }

        uint64_t maxPML5Entry = 1; // Always at least 1 entry

        uint64_t lastPML5Max = 1; // Always at least 1 entry
        uint64_t maxPML4Entry = 512;

        uint64_t lastPDPMax = 512; // This will decrease to the correct size, but worst-case it will be 512 and account for exactly 512GB RAM
        uint64_t maxPDPEntry = 512;

        while(maxRAM > (256ULL << 40))
        {
            maxPML5Entry++;
            maxRAM -= (256ULL << 40);
        }

        if(maxPML5Entry > 512)
        {
            maxPML5Entry = 512; //Cap it if there is an insane amount of RAM available
        }

        if(maxRAM)
        {
            while(maxRAM > (512ULL << 30))
            {
                maxPML5Entry++;
                maxRAM -= (512ULL << 30);
            }

            if(maxPML4Entry > 512)
            {
                maxPML4Entry = 512; //Cap it if there is an enourmous amount of RAM available
            }

            if(maxRAM)
            {
                //Final table may not be full
                lastPDPMax = ( (maxRAM + ((1 << 30) - 1)) & (~0ULL << 30) ) >> 30; // Catch any extra RAM into one more page

                if(lastPDPMax > 512) // Extreme case RAM size truncation
                {
                    lastPDPMax = 512;
                }
            }
        }

        // Now we have everything we need to know how much space the page tables are going to consume
        uint64_t pml4_space = PAGE_TABLE_SIZE*maxPML5Entry;
        uint64_t pdp_space = pml4_space*maxPML4Entry;

        asm volatile("mov %[dest], %%cr4"
            : // Outputs
            : [dest] "r" (reg3) // Inputs
            : // Clobbers
        );
    }

#ifdef DEBUG_PIOUS
    PrintDebugMessage("System Initialized\n");
#endif
}

void Abort(uint64_t errorCode)
{
    #ifdef DEBUG_PIOUS
        PrintErrorCode(errorCode, "");
    #else
        PrintString("ERROR", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color);
    #endif

    asm volatile("hlt");
}

//TODO: Use hardware acceration
int16_t CompareMemory(const void * addr1, const void * addr2, uint64_t length)
{
    for(; length > 0; length--)
    {
        if(*((uint8_t *)addr1) != *((uint8_t *)addr2))
            return *((uint8_t *)addr2) - *((uint8_t *)addr1);
    }
    return 0;
}