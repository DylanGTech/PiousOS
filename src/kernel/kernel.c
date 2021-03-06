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
#include "kernel/drivers.h"

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
    
    asm volatile (
            "mov sp, %[new_stack_base]\n\t"
            : // No outputs
            : [new_stack_base] "r" (kernel_stack + STACK_SIZE) // Inputs;
            : // Clobbers
    );
    
#endif

    InitializeSystem(LP);

    PrintString("Hello!\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor);
    PrintString("Stack Address: 0x%lX\n", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, kernel_stack);
    
    InitializeDrivers(LP->ConfigTables, LP->Number_of_ConfigTables);

    while(1)
    {

    }
}