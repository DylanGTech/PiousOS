#ifndef _System_H
#define _System_H 1

#define PAGE_TABLE_SIZE 512*8

#include "bootloader/bootloader.h"

//Descritor formats
typedef struct __attribute__ ((packed)) {
  UINT16 Limit;
  UINT64 BaseAddress;
} DT_STRUCT;

//Task-State Segment x86_64
typedef struct __attribute__ ((packed)) {
    UINT32 Reserved_0;

    UINT64 RSP0; //RSP Ring 0
    UINT64 RSP1; //RSP Ring 1
    UINT64 RSP2; //RSP Ring 2
    
    UINT64 Reserved_1;
    
    //Interrupt Stack Pointers
    UINT64 IST0;
    UINT64 IST1;
    UINT64 IST2;
    UINT64 IST3;
    UINT64 IST4;
    UINT64 IST5;
    UINT64 IST6;
    UINT64 IST7;
    
    UINT64 Reserved_2;
    UINT16 Reserved_3;
    
    UINT16 IO_Map_Base; //The address of the I/O permissions bitmap
} TSS64_STRUCT;

typedef struct __attribute__ ((packed)) {
  UINT16 SegmentLimit1; // Low bits, SegmentLimit2andMisc2 has MSBs (it's a 20-bit value)
  UINT16 BaseAddress1; // Low bits (15:0)
  UINT8  BaseAddress2; // Next bits (23:16)
  UINT8  Misc1; // Bits 0-3: segment/gate Type, 4: S, 5-6: DPL, 7: P
  UINT8  SegmentLimit2andMisc2; // Bits 0-3: seglimit2, 4: Available, 5: L, 6: D/B, 7: G
  UINT8  BaseAddress3; // More significant bits (31:24)
  UINT32 BaseAddress4; // Most significant bits (63:32)
  UINT8  Reserved;
  UINT8  Misc3andReserved2; // Low 4 bits are 0, upper 4 bits are reserved
  UINT16 Reserved3;
} TSS_LDT_ENTRY_STRUCT;

typedef struct __attribute__ ((packed)) {
  UINT16 SegmentLimit1; // Low bits, SegmentLimit2andMisc2 has MSBs (it's a 20-bit value)
  UINT16 BaseAddress1; // Low bits (15:0)
  UINT8  BaseAddress2; // Next bits (23:16)
  UINT8  Misc1; // Bits 0-3: segment/gate Type, 4: S, 5-6: DPL, 7: P
  UINT8  SegmentLimit2andMisc2; // Bits 0-3: seglimit2, 4: Available, 5: L, 6: D/B, 7: G
  UINT8  BaseAddress3; // Most significant bits (31:24)
} GDT_ENTRY_STRUCT; // This whole struct can fit in a 64-bit int. Printf %lx could give the whole thing.

typedef struct __attribute__ ((packed)) {
  UINT16 Offset1; // Low offset bits (15:0)
  UINT16 SegmentSelector;
  UINT8  ISTandZero; // Low bits (2:0) are IST, (7:3) should be set to 0
  UINT8  Misc; // Bits 0-3: segment/gate Type, 4: S (set to 0), 5-6: DPL, 7: P
  UINT16 Offset2; // Middle offset bits (31:16)
  UINT32 Offset3; // Upper offset bits (63:32)
  UINT32 Reserved;
} IDT_GATE_STRUCT; // Interrupt and trap gates use this format

void InitializeISR();

#endif