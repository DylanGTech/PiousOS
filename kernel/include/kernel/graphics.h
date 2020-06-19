#ifndef _Graphics_H
#define _Graphics_H 1


#include "kernel/kernel.h"

typedef struct TextDisplaySettings {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE  defaultGPU;       // Default EFI GOP output device from GPUArray (should be GPUArray[0] if there's only 1)
	UINT32                             fontColor;       // Default font color
	UINT32                             highlightColor;  // Default highlight color
    UINT32                             backgroundColor; // Default background color
    UINT8                              scale;
    UINT32                             index;            // Global string index for printf, etc. to keep track of cursor's postion in the framebuffer
} TextDisplaySettings;

extern TextDisplaySettings mainTextDisplaySettings;


void PrintString(unsigned char * str, UINT32 foregroundcolor, UINT32 backgroundColor, ...);
void PrintCharacter(unsigned char chr, UINT32 foregroundColor, UINT32 backgroundColor);
void ScrollUp();
void InitializeDisplay();
void ColorScreen(UINT32 color);

#ifdef DEBUG_PIOUS
void PrintDebugMessage(unsigned char * str);
void PrintErrorCode(unsigned long code, unsigned char * message);
#endif

#endif