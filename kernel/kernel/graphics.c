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

#include "kernel/kernel.h"
#include "bootloader/elf.h"


#include "kernel/graphics.h"
#include "kernel/font_8x8.h"


TextDisplaySettings mainTextDisplaySettings;

void Initialize_Display(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU)
{
    mainTextDisplaySettings.defaultGPU = GPU;
    mainTextDisplaySettings.font_color = 0x00FFFFFF;
    mainTextDisplaySettings.highlight_color = 0x0033FF33;
    mainTextDisplaySettings.background_color = 0x00000000;

    UINT32 pixels = (GPU.Info->PixelsPerScanLine - (GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution)) * GPU.Info->VerticalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    if(pixels > 1920*1080) mainTextDisplaySettings.scale = 2;
    else mainTextDisplaySettings.scale = 1;


    mainTextDisplaySettings.index = 0;

    ColorScreen(mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.background_color);
}


#ifdef DEBUG_PIOUS
void PrintDebugMessage(unsigned char * str)
{
    PrintString("[DEBUG]", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.highlight_color, mainTextDisplaySettings.background_color);
    PrintString(" ", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color);
    PrintString(str, mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color);
}

void PrintErrorCode(unsigned long code, unsigned char * message)
{
    PrintString("[ERROR]", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.highlight_color, mainTextDisplaySettings.background_color);
    PrintString(" ", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color);
    
    if(*message != '\0')
    {
        PrintString(message, mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color);
        PrintString(" - ", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color);
    }

    //TODO: Change to unsigned long for error codes
    PrintString("Error Code: %d", mainTextDisplaySettings.defaultGPU, mainTextDisplaySettings.font_color, mainTextDisplaySettings.background_color, (int)code);


}
#endif

void PrintCharacter(unsigned char chr, EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 foregroundColor, UINT32 backgroundColor)
{

    UINT32 backporch = GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    UINT32 rowSize = GPU.Info->VerticalResolution / (8 * mainTextDisplaySettings.scale);
    UINT32 colSize = (GPU.Info->PixelsPerScanLine - backporch) / (8 * mainTextDisplaySettings.scale);

    UINT32 col;
    UINT32 row;
            
            
    UINT8 i;
    UINT8 j;
    UINT8 k;

    for(i = 0; i < 64; i++)
    {
        if((font8x8_basic[chr][i % 8] >> (i / 8)) & 0x01)
        {
            for(j = 0; j < mainTextDisplaySettings.scale; j++)
            {
                for(k = 0; k < mainTextDisplaySettings.scale; k++)
                {
                    *(UINT32*)(GPU.FrameBufferBase + 4 * (GPU.Info->PixelsPerScanLine * (mainTextDisplaySettings.index / colSize * (8 * mainTextDisplaySettings.scale) + (i % 8) * mainTextDisplaySettings.scale + j) + (mainTextDisplaySettings.index % colSize * (8 * mainTextDisplaySettings.scale) + (i / 8) * mainTextDisplaySettings.scale + k))) = foregroundColor;
                }
            }
        }
        else
        {
            for(j = 0; j < mainTextDisplaySettings.scale; j++)
            {
                for(k = 0; k < mainTextDisplaySettings.scale; k++)
                {
                    *(UINT32*)(GPU.FrameBufferBase + 4 * (GPU.Info->PixelsPerScanLine * (mainTextDisplaySettings.index / colSize * (8 * mainTextDisplaySettings.scale) + (i % 8) * mainTextDisplaySettings.scale + j) + (mainTextDisplaySettings.index % colSize * (8 * mainTextDisplaySettings.scale) + (i / 8) * mainTextDisplaySettings.scale + k))) = backgroundColor;
                }
            }
        }
                
    }

    if(mainTextDisplaySettings.index == rowSize * colSize - 1)
    {
        ScrollUp(GPU);
        mainTextDisplaySettings.index = (rowSize - 1) * colSize;
    }
    else mainTextDisplaySettings.index++;
}

void ScrollUp(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU)
{
    UINT32 backporch = GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    UINT32 rowSize = GPU.Info->VerticalResolution / (8 * mainTextDisplaySettings.scale);
    UINT32 colSize = (GPU.Info->PixelsPerScanLine - backporch) / (8 * mainTextDisplaySettings.scale);

    UINT32 col;
    UINT32 row;


    for(row = 0; row < GPU.Info->VerticalResolution - 8 * mainTextDisplaySettings.scale; row++)
        {
            for(col = 0; col < (GPU.Info->PixelsPerScanLine - backporch); col++)
            {
                *(UINT32*)(GPU.FrameBufferBase + 4 * (GPU.Info->PixelsPerScanLine * row + col)) = *(UINT32*)(GPU.FrameBufferBase + 4 * (GPU.Info->PixelsPerScanLine * (row + 8 * mainTextDisplaySettings.scale) + col));
                *(UINT32*)(GPU.FrameBufferBase + 4 * (GPU.Info->PixelsPerScanLine * (row + 8 * mainTextDisplaySettings.scale) + col)) = mainTextDisplaySettings.background_color;        
            }
        }
        mainTextDisplaySettings.index = (rowSize - 1) * colSize;
}


void PrintString(unsigned char * str, EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 foregroundColor, UINT32 backgroundColor, ...)
{
    va_list valist;
    UINT8 num_args = 0;

    unsigned char * str_scanner = str;
    while(*str_scanner != '\0')
    {
        if(*str_scanner == '%')
        {
            str_scanner++;
            switch (*str_scanner)
            {
            case '%':
                break;
            case 'c':
            case 'd':
            case 'i':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'o':
            case 's':
            case 'u':
            case 'x':
            case 'X':
            case 'p':
            case 'n':
                num_args++;
                break;
            default:
                break;
            }
        }

        str_scanner++;
    }




    UINT32 backporch = GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    UINT32 rowSize = GPU.Info->VerticalResolution / (8 * mainTextDisplaySettings.scale);
    UINT32 colSize = (GPU.Info->PixelsPerScanLine - backporch) / (8 * mainTextDisplaySettings.scale);

    int value;
    unsigned char places;
    int value_cpy;
    int divisor;
    unsigned char i;
    unsigned char p;

    va_start(valist, num_args);

    while(*str != '\0')
    {
        if(*str == '%')
        {
            str++;
            switch (*str)
            {
            case '%':
                PrintCharacter(*str, GPU, foregroundColor, backgroundColor);
                break;
            case 'c':
                PrintCharacter(va_arg(valist, int), GPU, foregroundColor, backgroundColor);
                break;
            case 'd':
            case 'i':
                value = va_arg(valist, int);

                if(value < 0)
                {
                    PrintCharacter('-', GPU, foregroundColor, backgroundColor);
                    value *= -1;
                }
                places = 0;
                value_cpy = value;
                while(value_cpy > 0)
                {
                    value_cpy /= 10;
                    places++;
                }

                if(places == 0)
                    PrintCharacter('0', GPU, foregroundColor, backgroundColor);
                else
                {
                    while(places > 0)
                    {
                        divisor = 1;
                        for(i = 1; i < places; i++) divisor *= 10;
                        p = (value / divisor) % 10;
                        
                        PrintCharacter(p + 48, GPU, foregroundColor, backgroundColor);
                        places--;
                    }
                }
                break;
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'o':
            case 's':
            case 'u':
            case 'x':
            case 'X':
            case 'p':
            case 'n':
                break;
            default:
                break;
            }
        }
        else if(*str == '\n')
        {
            if(mainTextDisplaySettings.index % colSize >= rowSize - 1)
            {
                ScrollUp(GPU);
                mainTextDisplaySettings.index = colSize * (rowSize - 1);
            }
            else mainTextDisplaySettings.index = (mainTextDisplaySettings.index / colSize + 1) * colSize;
        }
        else
        {
            PrintCharacter(*str, GPU, foregroundColor, backgroundColor);
        }
        str++;
    }

    va_end(valist);
}


void ColorScreen(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 color)
{
    UINT32 row, col;
    UINT32 backporch = GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    for (row = 0; row < GPU.Info->VerticalResolution; row++)
    {
        for (col = 0; col < (GPU.Info->PixelsPerScanLine - backporch); col++) // Per UEFI Spec 2.7 Errata A, framebuffer address 0 coincides with the top leftmost pixel. i.e. screen padding is only HorizontalResolution + porch.
        {
            *(UINT32*)(GPU.FrameBufferBase + 4 * (GPU.Info->PixelsPerScanLine * row + col)) = color; // The thing at FrameBufferBase is an address pointing to UINT32s. FrameBufferBase itself is a 64-bit number.
        }
    }
}