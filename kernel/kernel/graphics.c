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
    mainTextDisplaySettings.fontColor = 0x00FFFFFF;
    mainTextDisplaySettings.highlightColor = 0x0033FF33;
    mainTextDisplaySettings.backgroundColor = 0x00000000;

    UINT32 pixels = (GPU.Info->PixelsPerScanLine - (GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution)) * GPU.Info->VerticalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    if(pixels >= 1920*1080) mainTextDisplaySettings.scale = 4;
    else if(pixels >= 960*540) mainTextDisplaySettings.scale = 2;
    else mainTextDisplaySettings.scale = 1;


    mainTextDisplaySettings.index = 0;

    ColorScreen(mainTextDisplaySettings.backgroundColor);
}


#ifdef DEBUG_PIOUS
void PrintDebugMessage(unsigned char * str)
{
    PrintString("[DEBUG]", mainTextDisplaySettings.highlightColor, mainTextDisplaySettings.backgroundColor);
    PrintString(" ", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor);
    PrintString(str, mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor);
}

void PrintErrorCode(unsigned long code, unsigned char * message)
{
    PrintString("[ERROR]", mainTextDisplaySettings.highlightColor, mainTextDisplaySettings.backgroundColor);
    PrintString(" ", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor);
    
    if(*message != '\0')
    {
        PrintString(message, mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor);
        PrintString(" - ", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor);
    }

    //TODO: Change to unsigned long for error codes
    PrintString("Error Code: %d", mainTextDisplaySettings.fontColor, mainTextDisplaySettings.backgroundColor, (int)code);


}
#endif

void PrintCharacter(unsigned char chr, UINT32 foregroundColor, UINT32 backgroundColor)
{

    UINT32 backporch = mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - mainTextDisplaySettings.defaultGPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    UINT32 rowSize = mainTextDisplaySettings.defaultGPU.Info->VerticalResolution / (8 * mainTextDisplaySettings.scale);
    UINT32 colSize = (mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - backporch) / (8 * mainTextDisplaySettings.scale);

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
                    *(UINT32*)(mainTextDisplaySettings.defaultGPU.FrameBufferBase + 4 * (mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine * (mainTextDisplaySettings.index / colSize * (8 * mainTextDisplaySettings.scale) + (i % 8) * mainTextDisplaySettings.scale + j) + (mainTextDisplaySettings.index % colSize * (8 * mainTextDisplaySettings.scale) + (i / 8) * mainTextDisplaySettings.scale + k))) = foregroundColor;
                }
            }
        }
        else
        {
            for(j = 0; j < mainTextDisplaySettings.scale; j++)
            {
                for(k = 0; k < mainTextDisplaySettings.scale; k++)
                {
                    *(UINT32*)(mainTextDisplaySettings.defaultGPU.FrameBufferBase + 4 * (mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine * (mainTextDisplaySettings.index / colSize * (8 * mainTextDisplaySettings.scale) + (i % 8) * mainTextDisplaySettings.scale + j) + (mainTextDisplaySettings.index % colSize * (8 * mainTextDisplaySettings.scale) + (i / 8) * mainTextDisplaySettings.scale + k))) = backgroundColor;
                }
            }
        }
                
    }

    if(mainTextDisplaySettings.index == rowSize * colSize - 1)
    {
        ScrollUp();
        mainTextDisplaySettings.index = (rowSize - 1) * colSize;
    }
    else mainTextDisplaySettings.index++;
}

void ScrollUp()
{
    UINT32 backporch = mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - mainTextDisplaySettings.defaultGPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    UINT32 rowSize = mainTextDisplaySettings.defaultGPU.Info->VerticalResolution / (8 * mainTextDisplaySettings.scale);
    UINT32 colSize = (mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - backporch) / (8 * mainTextDisplaySettings.scale);

    UINT32* startAddress = (UINT32*)mainTextDisplaySettings.defaultGPU.FrameBufferBase;
    UINT32 incrementation = mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine * 8 * mainTextDisplaySettings.scale;
    UINT32* endAddress = startAddress + mainTextDisplaySettings.defaultGPU.Info->VerticalResolution * mainTextDisplaySettings.defaultGPU.Info->HorizontalResolution - incrementation;

    UINT32* addr;
    for(addr = startAddress; addr != endAddress; addr++)
    {
        *addr = *(addr + incrementation);
    }
    
    for(; addr != startAddress + mainTextDisplaySettings.defaultGPU.Info->VerticalResolution * mainTextDisplaySettings.defaultGPU.Info->HorizontalResolution; addr++)
        *addr = mainTextDisplaySettings.backgroundColor;
}

void PrintString(unsigned char * str, UINT32 foregroundColor, UINT32 backgroundColor, ...)
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
            case 'h':
            case 'l':
                str_scanner++;
                num_args++;
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

    UINT32 backporch = mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - mainTextDisplaySettings.defaultGPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    UINT32 rowSize = mainTextDisplaySettings.defaultGPU.Info->VerticalResolution / (8 * mainTextDisplaySettings.scale);
    UINT32 colSize = (mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - backporch) / (8 * mainTextDisplaySettings.scale);

    unsigned long value;
    unsigned long value_cpy;

    unsigned char places;
    unsigned long divisor;
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
                PrintCharacter(*str, foregroundColor, backgroundColor);
                break;
            case 'c':
                PrintCharacter(va_arg(valist, int), foregroundColor, backgroundColor);
                break;
            
            case 'l':
                str++;
                switch (*str)
                {
                case 'd':
                case 'i':
                    value = va_arg(valist, long);

                    if((signed long)value < 0)
                    {
                        PrintCharacter('-', foregroundColor, backgroundColor);
                        value = (unsigned long)((signed long)value * -1);
                        //value ^= (1UL << 63);


                        if((signed long)value == 0) //Check for max unsigned value, since the following arithmatic won't work on it after flipping the sign bit
                        {
                            PrintString("9223372036854775808", foregroundColor, backgroundColor);
                            break;
                        }
                    }
                    places = 0;
                    value_cpy = value;
                    while((signed long)value_cpy > 0)
                    {
                        value_cpy = (signed long)value_cpy / 10;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 10;
                            p = ((signed long)value / divisor) % 10;
                        
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'o':
                    value = va_arg(valist, unsigned long);

                    places = 0;
                    value_cpy = value;
                    while(value_cpy > 0)
                    {
                        value_cpy /= 8;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 8;
                            p = (value / divisor) % 8;
                        
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'u':
                    value = va_arg(valist, unsigned long);

                    places = 0;
                    value_cpy = value;
                    while(value_cpy > 0)
                    {
                        value_cpy /= 10;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 10;
                            p = (value / divisor) % 10;
                        
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'x':
                    value = va_arg(valist, unsigned long);

                    places = 0;
                    value_cpy = value;
                    while(value_cpy > 0)
                    {
                        value_cpy /= 16;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 16;
                            p = (value / divisor) % 16;

                            if(p > 9)
                                PrintCharacter(p + 87, foregroundColor, backgroundColor);
                            else
                                PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'X':
                    value = va_arg(valist, unsigned long);

                    places = 0;
                    value_cpy = value;
                    while(value_cpy > 0)
                    {
                        value_cpy /= 16;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 16;
                            p = (value / divisor) % 16;

                            if(p > 9)
                                PrintCharacter(p + 66, foregroundColor, backgroundColor);
                            else
                                PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                default:
                    break;
                }

                break;
            case 'h':
                str++;
                switch (*str)
                {
                case 'd':
                case 'i':
                    value = (signed short)va_arg(valist, int);

                    if((signed short)value < 0)
                    {
                        PrintCharacter('-', foregroundColor, backgroundColor);
                        value = (unsigned long)((signed short)value * -1);
                        //value ^= (1UL << 15);

                        if((signed short)value == 0) //Check for max unsigned value, since the following arithmatic won't work on it after flipping the sign bit
                        {
                            PrintString("32768", foregroundColor, backgroundColor); //Check for max unsigned value, since the following arithmatic won't work on it after flipping the sign bit
                            break;
                        }
                    }
                    places = 0;
                    value_cpy = value;
                    while((signed short)value_cpy > 0)
                    {
                        value_cpy = (signed short)value_cpy / 10;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 10;
                            p = ((signed short)value / divisor) % 10;
                        
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'o':
                    value = (unsigned short)va_arg(valist, unsigned int);

                    places = 0;
                    value_cpy = value;
                    while((unsigned short)value_cpy > 0)
                    {
                        value_cpy = (unsigned short)value_cpy / 8;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 8;
                            p = ((unsigned short)value / divisor) % 8;
                        
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 's':
                case 'u':
                    value = (unsigned short)va_arg(valist, unsigned int);

                    places = 0;
                    value_cpy = value;
                    while((unsigned short)value_cpy > 0)
                    {
                        value_cpy = (unsigned short)value_cpy / 10;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 10;
                            p = ((unsigned short)value / divisor) % 10;
                        
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'x':
                    value = (unsigned short)va_arg(valist, int);

                    places = 0;
                    value_cpy = value;
                    while((unsigned short)value_cpy > 0)
                    {
                        value_cpy = (unsigned short)value_cpy / 16;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 16;
                            p = ((unsigned short)value / divisor) % 16;

                            if(p > 9)
                                PrintCharacter(p + 87, foregroundColor, backgroundColor);
                            else
                                PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'X':
                    value = (unsigned short)va_arg(valist, unsigned int);

                    places = 0;
                    value_cpy = value;
                    while((unsigned short)value_cpy > 0)
                    {
                        value_cpy = (unsigned short)value_cpy / 16;
                        places++;
                    }

                    if(places == 0)
                        PrintCharacter('0', foregroundColor, backgroundColor);
                    else
                    {
                        while(places > 0)
                        {
                            divisor = 1;
                            for(i = 1; i < places; i++) divisor *= 16;
                            p = ((unsigned short)value / divisor) % 16;

                            if(p > 9)
                                PrintCharacter(p + 66, foregroundColor, backgroundColor);
                            else
                                PrintCharacter(p + 48, foregroundColor, backgroundColor);
                            places--;
                        }
                    }
                    break;
                case 'p':
                case 'n':
                    break;
                default:
                    break;
                }

                break;
            case 'd':
            case 'i':
                value = (signed int)va_arg(valist, int);

                if((signed int)value < 0)
                {
                    PrintCharacter('-', foregroundColor, backgroundColor);
                    value = (unsigned long)((signed int)value * -1);
                    //value ^= (1UL << 31); //Flip sign bit

                    if((signed int)value == 0) //Check for max unsigned value, since the following arithmatic won't work on it after flipping the sign bit
                    {
                        PrintString("2147483648", foregroundColor, backgroundColor);
                        break;
                    }
                }
                places = 0;
                value_cpy = value;
                while((signed int)value_cpy > 0)
                {
                    value_cpy = (signed int)value_cpy / 10;
                    places++;
                }

                if(places == 0)
                    PrintCharacter('0', foregroundColor, backgroundColor);
                else
                {
                    while(places > 0)
                    {
                        divisor = 1;
                        for(i = 1; i < places; i++) divisor *= 10;
                        p = ((signed int)value / divisor) % 10;
                        
                        PrintCharacter(p + 48, foregroundColor, backgroundColor);
                        places--;
                    }
                }
                break;
            case 'e':
            case 'E':
            case 'g':
            case 'G':
                break;
            case 'o':
                value = (unsigned int)va_arg(valist, unsigned int);

                places = 0;
                value_cpy = value;
                while((unsigned int)value_cpy > 0)
                {
                    value_cpy = (unsigned int)value_cpy / 8;
                    places++;
                }

                if(places == 0)
                    PrintCharacter('0', foregroundColor, backgroundColor);
                else
                {
                    while(places > 0)
                    {
                        divisor = 1;
                        for(i = 1; i < places; i++) divisor *= 8;
                        p = ((signed int)value / divisor) % 8;
                        
                        PrintCharacter(p + 48, foregroundColor, backgroundColor);
                        places--;
                    }
                }
                break;
            case 'u':
                value = (unsigned int)va_arg(valist, unsigned int);

                places = 0;
                value_cpy = value;
                while((unsigned int)value_cpy > 0)
                {
                    value_cpy = (unsigned int)value_cpy / 10;
                    places++;
                }

                if(places == 0)
                    PrintCharacter('0', foregroundColor, backgroundColor);
                else
                {
                    while(places > 0)
                    {
                        divisor = 1;
                        for(i = 1; i < places; i++) divisor *= 10;
                        p = ((unsigned int)value / divisor) % 10;
                        
                        PrintCharacter(p + 48, foregroundColor, backgroundColor);
                        places--;
                    }
                }
                break;
            case 'x':
                value = (unsigned int)va_arg(valist, unsigned int);

                places = 0;
                value_cpy = value;
                while((unsigned int)value_cpy > 0)
                {
                    value_cpy = (unsigned int)value_cpy / 16;
                    places++;
                }

                if(places == 0)
                    PrintCharacter('0', foregroundColor, backgroundColor);
                else
                {
                    while(places > 0)
                    {
                        divisor = 1;
                        for(i = 1; i < places; i++) divisor *= 16;
                        p = ((unsigned int)value / divisor) % 16;
                        
                        if(p > 9)
                            PrintCharacter(p + 87, foregroundColor, backgroundColor);
                        else
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                        places--;
                    }
                }
                break;
            case 'X':
                value = va_arg(valist, unsigned int);

                places = 0;
                value_cpy = value;
                while((unsigned int)value_cpy > 0)
                {
                    value_cpy = (unsigned int)value_cpy / 16;
                    places++;
                }

                if(places == 0)
                    PrintCharacter('0', foregroundColor, backgroundColor);
                else
                {
                    while(places > 0)
                    {
                        divisor = 1;
                        for(i = 1; i < places; i++) divisor *= 16;
                        p = (value / divisor) % 16;
                        
                        if(p > 9)
                            PrintCharacter(p + 55, foregroundColor, backgroundColor);
                        else
                            PrintCharacter(p + 48, foregroundColor, backgroundColor);
                        places--;
                    }
                }
                break;
            case 'p':
            case 'n':
                break;
            default:
                break;
            }
        }
        else if(*str == '\n')
        {
            if(mainTextDisplaySettings.index / colSize >= rowSize - 1)
            {
                ScrollUp();
                mainTextDisplaySettings.index = colSize * (rowSize - 1);
            }
            else mainTextDisplaySettings.index = (mainTextDisplaySettings.index / colSize + 1) * colSize;
        }
        else
        {
            PrintCharacter(*str, foregroundColor, backgroundColor);
        }
        str++;
    }

    va_end(valist);
}


void ColorScreen(UINT32 color)
{
    UINT32 row, col;
    UINT32 backporch = mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - mainTextDisplaySettings.defaultGPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    for (row = 0; row < mainTextDisplaySettings.defaultGPU.Info->VerticalResolution; row++)
    {
        for (col = 0; col < (mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine - backporch); col++) // Per UEFI Spec 2.7 Errata A, framebuffer address 0 coincides with the top leftmost pixel. i.e. screen padding is only HorizontalResolution + porch.
        {
            *(UINT32*)(mainTextDisplaySettings.defaultGPU.FrameBufferBase + 4 * (mainTextDisplaySettings.defaultGPU.Info->PixelsPerScanLine * row + col)) = color; // The thing at FrameBufferBase is an address pointing to UINT32s. FrameBufferBase itself is a 64-bit number.
        }
    }
}