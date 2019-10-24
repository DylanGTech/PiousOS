#ifndef _Graphics_H
#define _Graphics_H 1


#include "kernel/kernel.h"

typedef struct TextDisplaySettings {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  defaultGPU;       // Default EFI GOP output device from GPUArray (should be GPUArray[0] if there's only 1)
	UINT32                             font_color;       // Default font color
	UINT32                             highlight_color;  // Default highlight color
    UINT32                             background_color; // Default background color
    UINT8                              scale;
    UINT32                             index;            // Global string index for printf, etc. to keep track of cursor's postion in the framebuffer
} TextDisplaySettings;

extern TextDisplaySettings mainTextDisplaySettings;


void PrintString(unsigned char * str, EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 foregroundcolor, UINT32 backgroundColor, ...);
void PrintCharacter(unsigned char chr, EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 foregroundColor, UINT32 backgroundColor);
void ScrollUp(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU);
void Initialize_Display(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU);
void ColorScreen(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE GPU, UINT32 color);

#ifdef DEBUG_PIOUS
void PrintDebugMessage(unsigned char * str);
void PrintErrorCode(unsigned long code, unsigned char * message);
#endif

#endif