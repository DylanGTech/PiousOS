//==================================================================================================================================
//  Simple Kernel: Interrupt Handlers
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
// This file provides interrupt handlers for x86-64 CPUs. It is inspired by what Clang/LLVM and GCC use for functions marked with
// __attribute__((interrupt)). Unfortunately they do not both make the same interrupt code (nor do they support the same features for
// interrupt functions, e.g. GCC does not support interrupts with SSE/AVX, and Clang 7 & 8 do it way out of x86 spec), so doing it this
// way ensures the best of both worlds and a consistent inter-compiler experience. I have also incorporated a full register dump into
// interrupt routines, which neither compiler does since they only preserve CPU state with the minimum possible set of registers
// (interesting since the minimum possible methods without -O3 are only about 5 registers short of a full register dump onto the stack).
//
// This file does what GCC does and refers to interrupts without error codes 'interrupts' (designated by 'isr') and ones with error
// codes 'exceptions' (designated by 'exc'). Intel names the first 0-31 entries in the IDT (the predefined entries) exceptions, but names
// 32-255 interrupts. Note that only some of the first 32 entries actually have error codes, meaning most of the first 32 entries will be
// handled by the interrupt code in this file, despite technically being called exceptions. This file has been set up this way such that
// only 2 kinds of functions are needed (interrupt and exception) instead of 4: interrupt, interrupt with error code, exception, and
// exception with error code... And that does not even account for the extra naming complexity brought on by faults, aborts, and traps.
// Using only 2 names instead of all of that, I think, keeps it about as simple as it needs to be. Plus, working in 64-bit mode means
// inter-privilege level changes do not even apply--great since those could easily double the number of functions needed--and that the
// stack frame is auto-aligned by the CPU to 16-bytes since %rsp is pushed unconditionally.
//
// ...Ok, technically there are 3 kinds of interrupt/exception callers here. 2 are for the CPU ISRs/EXCs, and 1 for user-defined ISRs.
//
// See ISR.h for a step-by-step on how to add/modify a user-defined interrupt.
//

// Default ISR/EXC handlers, see macros section for defining unique handlers
.extern User_ISR_handler
.extern CPU_ISR_handler
.extern CPU_EXC_handler

.section .text

//----------------------------------------------------------------------------------------------------------------------------------
//  SAVE/RESTORE_ISR_REGISTERS: General Register Only ISR Handler
//----------------------------------------------------------------------------------------------------------------------------------
//
// Bog-standard interrupt handler macros with no instruction set extensions
//

.macro SAVE_ISR_REGISTERS
  pushq %rbp
  movq  %rsp, %rbp
  pushq %r15
  pushq %r14
  pushq %r13
  pushq %r12
  pushq %r11
  pushq %r10
  pushq %r9
  pushq %r8
  pushq %rdi
  pushq %rsi
  pushq %rdx
  pushq %rcx
  pushq %rbx
  pushq %rax
  cld
.endm

.macro RESTORE_ISR_REGISTERS
  popq %rax
  popq %rbx
  popq %rcx
  popq %rdx
  popq %rsi
  popq %rdi
  popq %r8
  popq %r9
  popq %r10
  popq %r11
  popq %r12
  popq %r13
  popq %r14
  popq %r15
  popq %rbp
.endm

//----------------------------------------------------------------------------------------------------------------------------------
//  isr_pusherX: Push Interrupt Number X Onto Stack and Call Handlers
//----------------------------------------------------------------------------------------------------------------------------------
//
// IDT points to these, which push the interrupt number onto the stack and call the relevant handler. Hard to know what interrupt is
// being handled without these.
//

//----------------------------//
// Function Macro Definitions //
//----------------------------//

//
// User-Defined Interrupts (no error code)
//

.macro USER_ISR_MACRO num:req name=User has_special_external_handler=0
.global \name\()_ISR_pusher\num

.if \has_special_external_handler
.extern \name\()_ISR_handler
.endif // Use default external User ISR handler otherwise

\name\()_ISR_pusher\num\():
  SAVE_ISR_REGISTERS
  pushq $\num // INTERRUPT_FRAME has ISR number at the base
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq \name\()_ISR_handler
  addq $8, %rsp // For isr_num
  RESTORE_ISR_REGISTERS
  iretq
.endm

//
// CPU Interrupts (no error code)
//

.macro CPU_ISR_MACRO num:req name=CPU has_special_external_handler=0
.global \name\()_ISR_pusher\num

.if \has_special_external_handler
.extern \name\()_ISR_handler
.endif // Use default external CPU ISR handler otherwise


\name\()_ISR_pusher\num\():
  SAVE_ISR_REGISTERS
  pushq $\num // INTERRUPT_FRAME has ISR number at the base
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq \name\()_ISR_handler
  addq $8, %rsp // For isr_num
  RESTORE_ISR_REGISTERS
  iretq
.endm

//
// CPU Exceptions (have error code)
//

.macro CPU_EXC_MACRO num:req name=CPU has_special_external_handler=0
.global \name\()_EXC_pusher\num

.if \has_special_external_handler
.extern \name\()_EXC_handler
.endif // Use default external CPU EXC handler otherwise


\name\()_EXC_pusher\num\():
  pushq $\num // Push isr_num here to maintain 16-byte alignment since we can
  SAVE_ISR_REGISTERS
#ifdef __MINGW32__
  movq %rsp, %rcx // MS ABI x86-64
#else
  movq %rsp, %rdi // SYSV ABI x86-64
#endif
  movl $0, %eax // Stack trace end
  callq \name\()_EXC_handler
  RESTORE_ISR_REGISTERS
  addq $16, %rsp // For isr_num and error code
  iretq
.endm

//-----------------------------------------//
// Function Definitions (Macro Invocation) //
//-----------------------------------------//

//
// Predefined System Interrupts and Exceptions
//
//
// These all have special external handlers, which is what that extra "1" indicates.
// The special handler format has a name of the form (name)_ISR_handler or (name)_EXC_handler, where (name) is the letters between
// the vector number and the "1" value. If the special handler is located in this file, omit the extra "1" since it just creates a
// .extern for the handler to indicate the handler is, for example, a C function.
//

CPU_ISR_MACRO 0 DE 1 // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
CPU_ISR_MACRO 1 DB 1 // Fault/Trap #DB: Debug Exception
CPU_ISR_MACRO 2 NMI 1 // NMI (Nonmaskable External Interrupt)
CPU_ISR_MACRO 3 BP 1 // Trap #BP: Breakpoint (INT3 instruction)
CPU_ISR_MACRO 4 OF 1 // Trap #OF: Overflow (INTO instruction)
CPU_ISR_MACRO 5 BR 1 // Fault #BR: BOUND Range Exceeded (BOUND instruction)
CPU_ISR_MACRO 6 UD 1 // Fault #UD: Invalid or Undefined Opcode
CPU_ISR_MACRO 7 NM 1 // Fault #NM: Device Not Available Exception

CPU_EXC_MACRO 8 DF 1 // Abort #DF: Double Fault (error code is always 0)

CPU_ISR_MACRO 9 CSO 1 // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

CPU_EXC_MACRO 10 TS 1 // Fault #TS: Invalid TSS
CPU_EXC_MACRO 11 NP 1 // Fault #NP: Segment Not Present
CPU_EXC_MACRO 12 SS 1 // Fault #SS: Stack Segment Fault
CPU_EXC_MACRO 13 GP 1 // Fault #GP: General Protection
CPU_EXC_MACRO 14 PF 1 // Fault #PF: Page Fault

CPU_ISR_MACRO 16 MF 1 // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

CPU_EXC_MACRO 17 AC 1 // Fault #AC: Alignment Check (error code is always 0)

CPU_ISR_MACRO 18 MC 1 // Abort #MC: Machine Check
CPU_ISR_MACRO 19 XM 1 // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
CPU_ISR_MACRO 20 VE 1 // Fault #VE: Virtualization Exception

CPU_EXC_MACRO 30 SX 1 // Fault #SX: Security Exception

//
// These are system reserved, if they trigger they will go to unhandled interrupt error
//

CPU_ISR_MACRO 15

CPU_ISR_MACRO 21
CPU_ISR_MACRO 22
CPU_ISR_MACRO 23
CPU_ISR_MACRO 24
CPU_ISR_MACRO 25
CPU_ISR_MACRO 26
CPU_ISR_MACRO 27
CPU_ISR_MACRO 28
CPU_ISR_MACRO 29

CPU_ISR_MACRO 31

//
// User-Defined Interrupts
//

// By default everything is set to USER_ISR_MACRO.

USER_ISR_MACRO 32
USER_ISR_MACRO 33
USER_ISR_MACRO 34
USER_ISR_MACRO 35
USER_ISR_MACRO 36
USER_ISR_MACRO 37
USER_ISR_MACRO 38
USER_ISR_MACRO 39
USER_ISR_MACRO 40
USER_ISR_MACRO 41
USER_ISR_MACRO 42
USER_ISR_MACRO 43
USER_ISR_MACRO 44
USER_ISR_MACRO 45
USER_ISR_MACRO 46
USER_ISR_MACRO 47
USER_ISR_MACRO 48
USER_ISR_MACRO 49
USER_ISR_MACRO 50
USER_ISR_MACRO 51
USER_ISR_MACRO 52
USER_ISR_MACRO 53
USER_ISR_MACRO 54
USER_ISR_MACRO 55
USER_ISR_MACRO 56
USER_ISR_MACRO 57
USER_ISR_MACRO 58
USER_ISR_MACRO 59
USER_ISR_MACRO 60
USER_ISR_MACRO 61
USER_ISR_MACRO 62
USER_ISR_MACRO 63
USER_ISR_MACRO 64
USER_ISR_MACRO 65
USER_ISR_MACRO 66
USER_ISR_MACRO 67
USER_ISR_MACRO 68
USER_ISR_MACRO 69
USER_ISR_MACRO 70
USER_ISR_MACRO 71
USER_ISR_MACRO 72
USER_ISR_MACRO 73
USER_ISR_MACRO 74
USER_ISR_MACRO 75
USER_ISR_MACRO 76
USER_ISR_MACRO 77
USER_ISR_MACRO 78
USER_ISR_MACRO 79
USER_ISR_MACRO 80
USER_ISR_MACRO 81
USER_ISR_MACRO 82
USER_ISR_MACRO 83
USER_ISR_MACRO 84
USER_ISR_MACRO 85
USER_ISR_MACRO 86
USER_ISR_MACRO 87
USER_ISR_MACRO 88
USER_ISR_MACRO 89
USER_ISR_MACRO 90
USER_ISR_MACRO 91
USER_ISR_MACRO 92
USER_ISR_MACRO 93
USER_ISR_MACRO 94
USER_ISR_MACRO 95
USER_ISR_MACRO 96
USER_ISR_MACRO 97
USER_ISR_MACRO 98
USER_ISR_MACRO 99
USER_ISR_MACRO 100
USER_ISR_MACRO 101
USER_ISR_MACRO 102
USER_ISR_MACRO 103
USER_ISR_MACRO 104
USER_ISR_MACRO 105
USER_ISR_MACRO 106
USER_ISR_MACRO 107
USER_ISR_MACRO 108
USER_ISR_MACRO 109
USER_ISR_MACRO 110
USER_ISR_MACRO 111
USER_ISR_MACRO 112
USER_ISR_MACRO 113
USER_ISR_MACRO 114
USER_ISR_MACRO 115
USER_ISR_MACRO 116
USER_ISR_MACRO 117
USER_ISR_MACRO 118
USER_ISR_MACRO 119
USER_ISR_MACRO 120
USER_ISR_MACRO 121
USER_ISR_MACRO 122
USER_ISR_MACRO 123
USER_ISR_MACRO 124
USER_ISR_MACRO 125
USER_ISR_MACRO 126
USER_ISR_MACRO 127
USER_ISR_MACRO 128
USER_ISR_MACRO 129
USER_ISR_MACRO 130
USER_ISR_MACRO 131
USER_ISR_MACRO 132
USER_ISR_MACRO 133
USER_ISR_MACRO 134
USER_ISR_MACRO 135
USER_ISR_MACRO 136
USER_ISR_MACRO 137
USER_ISR_MACRO 138
USER_ISR_MACRO 139
USER_ISR_MACRO 140
USER_ISR_MACRO 141
USER_ISR_MACRO 142
USER_ISR_MACRO 143
USER_ISR_MACRO 144
USER_ISR_MACRO 145
USER_ISR_MACRO 146
USER_ISR_MACRO 147
USER_ISR_MACRO 148
USER_ISR_MACRO 149
USER_ISR_MACRO 150
USER_ISR_MACRO 151
USER_ISR_MACRO 152
USER_ISR_MACRO 153
USER_ISR_MACRO 154
USER_ISR_MACRO 155
USER_ISR_MACRO 156
USER_ISR_MACRO 157
USER_ISR_MACRO 158
USER_ISR_MACRO 159
USER_ISR_MACRO 160
USER_ISR_MACRO 161
USER_ISR_MACRO 162
USER_ISR_MACRO 163
USER_ISR_MACRO 164
USER_ISR_MACRO 165
USER_ISR_MACRO 166
USER_ISR_MACRO 167
USER_ISR_MACRO 168
USER_ISR_MACRO 169
USER_ISR_MACRO 170
USER_ISR_MACRO 171
USER_ISR_MACRO 172
USER_ISR_MACRO 173
USER_ISR_MACRO 174
USER_ISR_MACRO 175
USER_ISR_MACRO 176
USER_ISR_MACRO 177
USER_ISR_MACRO 178
USER_ISR_MACRO 179
USER_ISR_MACRO 180
USER_ISR_MACRO 181
USER_ISR_MACRO 182
USER_ISR_MACRO 183
USER_ISR_MACRO 184
USER_ISR_MACRO 185
USER_ISR_MACRO 186
USER_ISR_MACRO 187
USER_ISR_MACRO 188
USER_ISR_MACRO 189
USER_ISR_MACRO 190
USER_ISR_MACRO 191
USER_ISR_MACRO 192
USER_ISR_MACRO 193
USER_ISR_MACRO 194
USER_ISR_MACRO 195
USER_ISR_MACRO 196
USER_ISR_MACRO 197
USER_ISR_MACRO 198
USER_ISR_MACRO 199
USER_ISR_MACRO 200
USER_ISR_MACRO 201
USER_ISR_MACRO 202
USER_ISR_MACRO 203
USER_ISR_MACRO 204
USER_ISR_MACRO 205
USER_ISR_MACRO 206
USER_ISR_MACRO 207
USER_ISR_MACRO 208
USER_ISR_MACRO 209
USER_ISR_MACRO 210
USER_ISR_MACRO 211
USER_ISR_MACRO 212
USER_ISR_MACRO 213
USER_ISR_MACRO 214
USER_ISR_MACRO 215
USER_ISR_MACRO 216
USER_ISR_MACRO 217
USER_ISR_MACRO 218
USER_ISR_MACRO 219
USER_ISR_MACRO 220
USER_ISR_MACRO 221
USER_ISR_MACRO 222
USER_ISR_MACRO 223
USER_ISR_MACRO 224
USER_ISR_MACRO 225
USER_ISR_MACRO 226
USER_ISR_MACRO 227
USER_ISR_MACRO 228
USER_ISR_MACRO 229
USER_ISR_MACRO 230
USER_ISR_MACRO 231
USER_ISR_MACRO 232
USER_ISR_MACRO 233
USER_ISR_MACRO 234
USER_ISR_MACRO 235
USER_ISR_MACRO 236
USER_ISR_MACRO 237
USER_ISR_MACRO 238
USER_ISR_MACRO 239
USER_ISR_MACRO 240
USER_ISR_MACRO 241
USER_ISR_MACRO 242
USER_ISR_MACRO 243
USER_ISR_MACRO 244
USER_ISR_MACRO 245
USER_ISR_MACRO 246
USER_ISR_MACRO 247
USER_ISR_MACRO 248
USER_ISR_MACRO 249
USER_ISR_MACRO 250
USER_ISR_MACRO 251
USER_ISR_MACRO 252
USER_ISR_MACRO 253
USER_ISR_MACRO 254
USER_ISR_MACRO 255

// Thank you Excel spreadsheet macros...
