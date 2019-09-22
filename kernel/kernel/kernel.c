#include "bootloader/elf.h"

#include "bootloader/bootloader.h"
#include "kernel/kernel.h"
#include "kernel/graphics.h"
#include "kernel/memory.h"

#define STACK_SIZE (1 << 20)

__attribute__((aligned(64))) static volatile unsigned char kernel_stack[STACK_SIZE] = {0};

__attribute__((naked)) void kernel_main(LOADER_PARAMS * LP) // Loader Parameters
{

#ifdef x86_64
    asm volatile ("leaq %[new_stack_base], %%rbp\n\t"
            "leaq %[new_stack_end], %%rsp \n\t"
            : // No outputs
            : [new_stack_base] "m" (kernel_stack[0]), [new_stack_end] "m" (kernel_stack[STACK_SIZE]) // Inputs; %rsp is decremented before use, so STACK_SIZE is used instead of STACK_SIZE - 1
            : // No clobbers
    );
#elif aarch64
    
#endif

    Initialize_System(LP);
    
    PrintString("Hello!\n", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color);
   
    while(1)
    {

    }
}