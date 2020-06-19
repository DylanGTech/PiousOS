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

#include "bootloader/elf.h"

#include "bootloader/bootloader.h"
#include "kernel/kernel.h"
#include "kernel/graphics.h"
#include "kernel/memory.h"

#define STACK_SIZE (1 << 20)


__attribute__((aligned(64))) static volatile unsigned char kernel_stack[STACK_SIZE];

void kernel_main(LOADER_PARAMS * LP) // Loader Parameters
{
#ifdef x86_64
    asm volatile ("leaq %[new_stack_base], %%rbp\n\t"
            "leaq %[new_stack_end], %%rsp \n\t"
            : // No outputs
            : [new_stack_base] "m" (kernel_stack[0]), [new_stack_end] "m" (kernel_stack[STACK_SIZE]) // Inputs; %rsp is decremented before use, so STACK_SIZE is used instead of STACK_SIZE - 1
            : // No clobbers
    );
#elif aarch64
    /*
    asm volatile (
            "ldr x7, =new_stack_base\n\t"
            "mov sp, x7\n\t"
            : // No outputs
            : [new_stack_base] "m" (kernel_stack[STACK_SIZE]) // Inputs;
            : // No clobbers
    );

    while(1) ;
    */
#endif

    InitializeSystem(LP);

    //uint64_t ram = GetTotalSystemRam();
    //PrintString("Total RAM: %lu bytes (about %u MiB)\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, ram, ram / 1024 / 1024);

    PrintString("Hello!\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor);
    //PrintString("MemMap Address: 0x%lX\nMemMap Size: %lu bytes\nDescriptor Size: %lu bytes\nDescriptor Version: %hu\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, (unsigned long)mainMemorySettings.memMap, (unsigned long)mainMemorySettings.memMapSize, (unsigned long)mainMemorySettings.memMapDescriptorSize, (unsigned short)mainMemorySettings.memMapDescriptorVersion);
    
    while(1)
    {

    }
}