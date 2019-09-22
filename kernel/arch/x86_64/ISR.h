// From KNNSpeed's "Simple Kernel":
// https://github.com/KNNSpeed/Simple-Kernel
// V0.z, 7/28/2019


//==================================================================================================================================
//  Simple Kernel: Interrupt Structures Header
//==================================================================================================================================
//
// Version 0.9
//
// Author:
//  KNNSpeed
//
// Source Code:
//  https://github.com/KNNSpeed/Simple-Kernel
//
// This file provides structures for interrupt handlers, and they reflect the stack as set up in ISR.S.
//
// *How to Add/Modify a User-Defined Interrupt:*
//
// To add an interrupt or exception (here, exception refers to interrupt with error code--not "the first 32 entries in the IDT"):
//
//  1) Ensure the macro used in ISR.S is correct for the desired interrupt number
//  2) Ensure the extern references the correct function at the bottom of this file
//  3) In the Setup_IDT() function in System.c, ensure set_interrupt_entry() is correct for the desired interrupt number (if you want
//     a trap instead, call set_trap_entry() instead of set_interrupt_entry() with the same arguments)
//  4) Add a "case (interrupt number):" statement in the correct handler function in System.c
//
// These are the 3 pathways for handlers, depending on which outcome is desired (just replace "(num)" with a number 32-255, since 0-31 are architecturally reserved):
//
// For user-defined notifications:
//  a. USER_ISR_MACRO (num) --> extern void User_ISR_pusher(num) --> set_interrupt_entry( (num), (uint64_t)User_ISR_pusher(num) ) --> "case (num):" in User_ISR_handler()
//    >> This assumes a special dedicated handler isn't desired, otherwise the macro syntax is slightly different as described in the macro section of ISR.S.
//
// For generic CPU architectural notifications:
//  b. CPU_ISR_MACRO (num) --> extern void CPU_ISR_pusher(num) --> set_interrupt_entry( (num), (uint64_t)CPU_ISR_pusher(num) ) --> CPU_ISR_handler()
//  c. CPU_EXC_MACRO (num) --> extern void CPU_EXC_pusher(num) --> set_interrupt_entry( (num), (uint64_t)CPU_EXC_pusher(num) ) --> CPU_EXC_handler()
//
// Note again that set_interrupt_entry() can be replaced by set_trap_entry() if desired. The difference is that traps don't clear IF in
// %rflags, which allows maskable interrupts to trigger during other interrupts instead of double-faulting.
//

#ifndef _ISR_H
#define _ISR_H

#include "kernel/kernel.h"
#include "kernel/memory.h"

// Intel Architecture Manual Vol. 3A, Fig. 6-4 (Stack Usage on Transfers to Interrupt and Exception-Handling Routines)
// and Fig. 6-8 (IA-32e Mode Stack Usage After Privilege Level Change)
// Note the order of these structs with respect to the stack. Note that 64-bit pushes SS:RSP unconditionally.
typedef struct __attribute__ ((packed)) {
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} INTERRUPT_FRAME_X64;

// Exception codes are pushed before rip (and so get popped first)
typedef struct __attribute__ ((packed)) {
  UINT64 error_code;
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} EXCEPTION_FRAME_X64;

// The below interrupt frames are set up by ISR.S

// All-in-one structure for interrupts
// ISRs save the state up to where the ISR was called, so a regdump is accessible
// Though it might not always be needed, a minimal ISR is only 5 registers away from a full dump anyways, so might as well just get the whole thing.
typedef struct __attribute__ ((packed)) {
  // ISR identification number pushed by ISR.S
  UINT64 isr_num;

  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} INTERRUPT_FRAME;

// All-in-one structure for exceptions
typedef struct __attribute__ ((packed)) {
  // Register save pushed by ISR.S
  UINT64 rax;
  UINT64 rbx;
  UINT64 rcx;
  UINT64 rdx;
  UINT64 rsi;
  UINT64 rdi;
  UINT64 r8;
  UINT64 r9;
  UINT64 r10;
  UINT64 r11;
  UINT64 r12;
  UINT64 r13;
  UINT64 r14;
  UINT64 r15;
  UINT64 rbp;

  // ISR identification number pushed by ISR.S
  UINT64 isr_num;
  // Exception error code pushed by CPU
  UINT64 error_code;

  // Standard x86-64 interrupt stack frame
  UINT64 rip;
  UINT64 cs;
  UINT64 rflags;
  UINT64 rsp;
  UINT64 ss;
} EXCEPTION_FRAME;

//
// Using XSAVE & XRSTOR:
//
// Per 13.7, XSAVE is invoked like this:
// XSAVE (address of first byte of XSAVE area)
// EAX and EDX should correspond to 0xE7 to save AVX512, AVX, SSE, x87
// EDX:EAX is an AND mask for XCR0.
//
// Per 13.8, XRSTOR is invoked in exactly the same way.
//
// Intel Architecture Manual Vol. 1, Section 13.4 (XSAVE Area)
typedef struct __attribute__((aligned(64), packed)) {
// Legacy region (first 512 bytes)
  // Legacy FXSAVE header
  UINT16 fcw;
  UINT16 fsw;
  UINT8 ftw;
  UINT8 Reserved1;
  UINT16 fop;
  UINT64 fip; // FCS is only for 32-bit
  UINT64 fdp; // FDS is only for 32-bit
  UINT32 mxcsr;
  UINT32 mxcsr_mask;

  // Legacy x87/MMX registers
  UINT64 st_mm_0[2];
  UINT64 st_mm_1[2];
  UINT64 st_mm_2[2];
  UINT64 st_mm_3[2];
  UINT64 st_mm_4[2];
  UINT64 st_mm_5[2];
  UINT64 st_mm_6[2];
  UINT64 st_mm_7[2];

  // SSE registers
  UINT64 xmm0[2];
  UINT64 xmm1[2];
  UINT64 xmm2[2];
  UINT64 xmm3[2];
  UINT64 xmm4[2];
  UINT64 xmm5[2];
  UINT64 xmm6[2];
  UINT64 xmm7[2];
  UINT64 xmm8[2];
  UINT64 xmm9[2];
  UINT64 xmm10[2];
  UINT64 xmm11[2];
  UINT64 xmm12[2];
  UINT64 xmm13[2];
  UINT64 xmm14[2];
  UINT64 xmm15[2];

  UINT8 Reserved2[48];// (463:416) are reserved.
  UINT8 pad[48]; // XSAVE doesn't use (511:464).

// AVX region
  // XSAVE header
  UINT64 xstate_bv; // CPU uses this to track what is saved in XSAVE area--init to 0 with the rest of the region and then don't modify it after.
  UINT64 xcomp_bv; // Only support for xcomp_bv = 0 is expressly provided here (it's the standard form of XSAVE/XRSTOR that all AVX CPUs must support)

  UINT64 Reserved3[6];

  // XSAVE Extended region
  // Only standard format support is provided here
  UINT8 extended_region[1]; // This depends on the values in EBX & EAX after cpuid EAX=0Dh, ECX=[state comp]

} XSAVE_AREA_LAYOUT;
// Note that XSAVES/XRSTORS, used for supervisor components, only includes Process Trace in addition to the standard "user" XSAVE features at the moment.
// The standard user XSAVE states include x87/SSE/AVX/AVX-512 and MPX/PKRU.

//------------------------------------------//
// References to functions defined in ISR.h //
//------------------------------------------//

//
// Predefined System Interrupts and Exceptions
//

extern void DE_ISR_pusher0(); // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
extern void DB_ISR_pusher1(); // Fault/Trap #DB: Debug Exception
extern void NMI_ISR_pusher2(); // NMI (Nonmaskable External Interrupt)
extern void BP_ISR_pusher3(); // Trap #BP: Breakpoint (INT3 instruction)
extern void OF_ISR_pusher4(); // Trap #OF: Overflow (INTO instruction)
extern void BR_ISR_pusher5(); // Fault #BR: BOUND Range Exceeded (BOUND instruction)
extern void UD_ISR_pusher6(); // Fault #UD: Invalid or Undefined Opcode
extern void NM_ISR_pusher7(); // Fault #NM: Device Not Available Exception

extern void DF_EXC_pusher8(); // Abort #DF: Double Fault (error code is always 0)

extern void CSO_ISR_pusher9(); // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

extern void TS_EXC_pusher10(); // Fault #TS: Invalid TSS
extern void NP_EXC_pusher11(); // Fault #NP: Segment Not Present
extern void SS_EXC_pusher12(); // Fault #SS: Stack Segment Fault
extern void GP_EXC_pusher13(); // Fault #GP: General Protection
extern void PF_EXC_pusher14(); // Fault #PF: Page Fault

extern void MF_ISR_pusher16(); // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

extern void AC_EXC_pusher17(); // Fault #AC: Alignment Check (error code is always 0)

extern void MC_ISR_pusher18(); // Abort #MC: Machine Check
extern void XM_ISR_pusher19(); // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
extern void VE_ISR_pusher20(); // Fault #VE: Virtualization Exception

extern void SX_EXC_pusher30(); // Fault #SX: Security Exception

//
// These are system reserved, if they trigger they will go to unhandled interrupt error
//

extern void CPU_ISR_pusher15();

extern void CPU_ISR_pusher21();
extern void CPU_ISR_pusher22();
extern void CPU_ISR_pusher23();
extern void CPU_ISR_pusher24();
extern void CPU_ISR_pusher25();
extern void CPU_ISR_pusher26();
extern void CPU_ISR_pusher27();
extern void CPU_ISR_pusher28();
extern void CPU_ISR_pusher29();

extern void CPU_ISR_pusher31();

//
// User-Defined Interrupts
//

// By default everything is set to USER_ISR_MACRO.

extern void User_ISR_pusher32();
extern void User_ISR_pusher33();
extern void User_ISR_pusher34();
extern void User_ISR_pusher35();
extern void User_ISR_pusher36();
extern void User_ISR_pusher37();
extern void User_ISR_pusher38();
extern void User_ISR_pusher39();
extern void User_ISR_pusher40();
extern void User_ISR_pusher41();
extern void User_ISR_pusher42();
extern void User_ISR_pusher43();
extern void User_ISR_pusher44();
extern void User_ISR_pusher45();
extern void User_ISR_pusher46();
extern void User_ISR_pusher47();
extern void User_ISR_pusher48();
extern void User_ISR_pusher49();
extern void User_ISR_pusher50();
extern void User_ISR_pusher51();
extern void User_ISR_pusher52();
extern void User_ISR_pusher53();
extern void User_ISR_pusher54();
extern void User_ISR_pusher55();
extern void User_ISR_pusher56();
extern void User_ISR_pusher57();
extern void User_ISR_pusher58();
extern void User_ISR_pusher59();
extern void User_ISR_pusher60();
extern void User_ISR_pusher61();
extern void User_ISR_pusher62();
extern void User_ISR_pusher63();
extern void User_ISR_pusher64();
extern void User_ISR_pusher65();
extern void User_ISR_pusher66();
extern void User_ISR_pusher67();
extern void User_ISR_pusher68();
extern void User_ISR_pusher69();
extern void User_ISR_pusher70();
extern void User_ISR_pusher71();
extern void User_ISR_pusher72();
extern void User_ISR_pusher73();
extern void User_ISR_pusher74();
extern void User_ISR_pusher75();
extern void User_ISR_pusher76();
extern void User_ISR_pusher77();
extern void User_ISR_pusher78();
extern void User_ISR_pusher79();
extern void User_ISR_pusher80();
extern void User_ISR_pusher81();
extern void User_ISR_pusher82();
extern void User_ISR_pusher83();
extern void User_ISR_pusher84();
extern void User_ISR_pusher85();
extern void User_ISR_pusher86();
extern void User_ISR_pusher87();
extern void User_ISR_pusher88();
extern void User_ISR_pusher89();
extern void User_ISR_pusher90();
extern void User_ISR_pusher91();
extern void User_ISR_pusher92();
extern void User_ISR_pusher93();
extern void User_ISR_pusher94();
extern void User_ISR_pusher95();
extern void User_ISR_pusher96();
extern void User_ISR_pusher97();
extern void User_ISR_pusher98();
extern void User_ISR_pusher99();
extern void User_ISR_pusher100();
extern void User_ISR_pusher101();
extern void User_ISR_pusher102();
extern void User_ISR_pusher103();
extern void User_ISR_pusher104();
extern void User_ISR_pusher105();
extern void User_ISR_pusher106();
extern void User_ISR_pusher107();
extern void User_ISR_pusher108();
extern void User_ISR_pusher109();
extern void User_ISR_pusher110();
extern void User_ISR_pusher111();
extern void User_ISR_pusher112();
extern void User_ISR_pusher113();
extern void User_ISR_pusher114();
extern void User_ISR_pusher115();
extern void User_ISR_pusher116();
extern void User_ISR_pusher117();
extern void User_ISR_pusher118();
extern void User_ISR_pusher119();
extern void User_ISR_pusher120();
extern void User_ISR_pusher121();
extern void User_ISR_pusher122();
extern void User_ISR_pusher123();
extern void User_ISR_pusher124();
extern void User_ISR_pusher125();
extern void User_ISR_pusher126();
extern void User_ISR_pusher127();
extern void User_ISR_pusher128();
extern void User_ISR_pusher129();
extern void User_ISR_pusher130();
extern void User_ISR_pusher131();
extern void User_ISR_pusher132();
extern void User_ISR_pusher133();
extern void User_ISR_pusher134();
extern void User_ISR_pusher135();
extern void User_ISR_pusher136();
extern void User_ISR_pusher137();
extern void User_ISR_pusher138();
extern void User_ISR_pusher139();
extern void User_ISR_pusher140();
extern void User_ISR_pusher141();
extern void User_ISR_pusher142();
extern void User_ISR_pusher143();
extern void User_ISR_pusher144();
extern void User_ISR_pusher145();
extern void User_ISR_pusher146();
extern void User_ISR_pusher147();
extern void User_ISR_pusher148();
extern void User_ISR_pusher149();
extern void User_ISR_pusher150();
extern void User_ISR_pusher151();
extern void User_ISR_pusher152();
extern void User_ISR_pusher153();
extern void User_ISR_pusher154();
extern void User_ISR_pusher155();
extern void User_ISR_pusher156();
extern void User_ISR_pusher157();
extern void User_ISR_pusher158();
extern void User_ISR_pusher159();
extern void User_ISR_pusher160();
extern void User_ISR_pusher161();
extern void User_ISR_pusher162();
extern void User_ISR_pusher163();
extern void User_ISR_pusher164();
extern void User_ISR_pusher165();
extern void User_ISR_pusher166();
extern void User_ISR_pusher167();
extern void User_ISR_pusher168();
extern void User_ISR_pusher169();
extern void User_ISR_pusher170();
extern void User_ISR_pusher171();
extern void User_ISR_pusher172();
extern void User_ISR_pusher173();
extern void User_ISR_pusher174();
extern void User_ISR_pusher175();
extern void User_ISR_pusher176();
extern void User_ISR_pusher177();
extern void User_ISR_pusher178();
extern void User_ISR_pusher179();
extern void User_ISR_pusher180();
extern void User_ISR_pusher181();
extern void User_ISR_pusher182();
extern void User_ISR_pusher183();
extern void User_ISR_pusher184();
extern void User_ISR_pusher185();
extern void User_ISR_pusher186();
extern void User_ISR_pusher187();
extern void User_ISR_pusher188();
extern void User_ISR_pusher189();
extern void User_ISR_pusher190();
extern void User_ISR_pusher191();
extern void User_ISR_pusher192();
extern void User_ISR_pusher193();
extern void User_ISR_pusher194();
extern void User_ISR_pusher195();
extern void User_ISR_pusher196();
extern void User_ISR_pusher197();
extern void User_ISR_pusher198();
extern void User_ISR_pusher199();
extern void User_ISR_pusher200();
extern void User_ISR_pusher201();
extern void User_ISR_pusher202();
extern void User_ISR_pusher203();
extern void User_ISR_pusher204();
extern void User_ISR_pusher205();
extern void User_ISR_pusher206();
extern void User_ISR_pusher207();
extern void User_ISR_pusher208();
extern void User_ISR_pusher209();
extern void User_ISR_pusher210();
extern void User_ISR_pusher211();
extern void User_ISR_pusher212();
extern void User_ISR_pusher213();
extern void User_ISR_pusher214();
extern void User_ISR_pusher215();
extern void User_ISR_pusher216();
extern void User_ISR_pusher217();
extern void User_ISR_pusher218();
extern void User_ISR_pusher219();
extern void User_ISR_pusher220();
extern void User_ISR_pusher221();
extern void User_ISR_pusher222();
extern void User_ISR_pusher223();
extern void User_ISR_pusher224();
extern void User_ISR_pusher225();
extern void User_ISR_pusher226();
extern void User_ISR_pusher227();
extern void User_ISR_pusher228();
extern void User_ISR_pusher229();
extern void User_ISR_pusher230();
extern void User_ISR_pusher231();
extern void User_ISR_pusher232();
extern void User_ISR_pusher233();
extern void User_ISR_pusher234();
extern void User_ISR_pusher235();
extern void User_ISR_pusher236();
extern void User_ISR_pusher237();
extern void User_ISR_pusher238();
extern void User_ISR_pusher239();
extern void User_ISR_pusher240();
extern void User_ISR_pusher241();
extern void User_ISR_pusher242();
extern void User_ISR_pusher243();
extern void User_ISR_pusher244();
extern void User_ISR_pusher245();
extern void User_ISR_pusher246();
extern void User_ISR_pusher247();
extern void User_ISR_pusher248();
extern void User_ISR_pusher249();
extern void User_ISR_pusher250();
extern void User_ISR_pusher251();
extern void User_ISR_pusher252();
extern void User_ISR_pusher253();
extern void User_ISR_pusher254();
extern void User_ISR_pusher255();


extern __attribute__((aligned(64))) uint64_t MinimalGDT[5];
//extern __attribute__((aligned(64))) TSS64_STRUCT tss64;


void User_ISR_handler(INTERRUPT_FRAME * i_frame);
void CPU_ISR_handler(INTERRUPT_FRAME * i_frame);

#endif /* _ISR_H */
