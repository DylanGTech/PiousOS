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
#endif

void PrintString(unsigned char * str, EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 foregroundColor, UINT32 backgroundColor)
{
    UINT32 backporch = GPU.Info->PixelsPerScanLine - GPU.Info->HorizontalResolution; // The area offscreen is the back porch. Sometimes it's 0.
    UINT32 rowSize = GPU.Info->VerticalResolution / (8 * mainTextDisplaySettings.scale);
    UINT32 colSize = (GPU.Info->PixelsPerScanLine - backporch) / (8 * mainTextDisplaySettings.scale);

    while(*str != '\0')
    {
        if(*str == '\n')
        {
            if(mainTextDisplaySettings.index % colSize >= rowSize - 1)
            {
                //TODO: Make scrolling function
                mainTextDisplaySettings.index = 0;
            }
            else mainTextDisplaySettings.index = (mainTextDisplaySettings.index / colSize + 1) * colSize;
        }
        else
        {
            UINT32 col;
            UINT32 row;
            
            
            UINT8 i;
            UINT8 j;
            UINT8 k;

            for(i = 0; i < 64; i++)
            {
                if((font8x8_basic[*str][i % 8] >> (i / 8)) & 0x01)
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
            else mainTextDisplaySettings.index++;
        }
        str++;
    }
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