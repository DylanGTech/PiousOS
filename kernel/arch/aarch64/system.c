#include "system.h"
#include "kernel/graphics.h"
#include "kernel/memory.h"


void InitializeSystem(LOADER_PARAMS * Parameters)
{
    InitializeMemory(Parameters->Memory_Map_Size, Parameters->Memory_Map_Size, Parameters->Memory_Map, Parameters->Memory_Map_Descriptor_Version);
    InitializeDisplay(Parameters->GPU_Configs->GPUArray[0]);

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

    //asm volatile("hlt 0");
    asm volatile(
    "halt:"
    "    b halt;"
    );
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