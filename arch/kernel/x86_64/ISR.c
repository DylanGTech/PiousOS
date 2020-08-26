#include "kernel/kernel.h"
#include "ISR.h"
#include "system.h"
//#include "kernel/EfiTypes.h"
//#include "kernel/EfiBind.h"
//#include "kernel/EfiErr.h"


static void SetInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void SetNMIInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void SetBPInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void SetMCInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void SetTrapEntry(uint64_t isrNum, uint64_t isrAddr);

__attribute__((aligned(64))) TSS64_STRUCT tss64 = {0};

__attribute__((aligned(64))) static IDT_GATE_STRUCT IDT_data[256] = {0};
__attribute__((aligned(64))) static volatile unsigned char NMI_stack[1 << 12] = {0};
__attribute__((aligned(64))) static volatile unsigned char DF_stack[1 << 12] = {0};
__attribute__((aligned(64))) static volatile unsigned char MC_stack[1 << 12] = {0};
__attribute__((aligned(64))) static volatile unsigned char BP_stack[1 << 12] = {0};

#define XSAVE_SIZE (1 << 13)

__attribute__((aligned(64))) static volatile unsigned char cpu_xsave_space[XSAVE_SIZE] = {0}; // Generic space for unhandled/unknown IDT vectors in the 0-31 range.
__attribute__((aligned(64))) static volatile unsigned char user_xsave_space[XSAVE_SIZE] = {0}; // For vectors 32-255, which can't preempt each other due to interrupt gating (IF in RFLAGS is cleared during ISR execution)


void InitializeISR()
{
    uint64_t reg;

    asm volatile("mov %%cr0, %[dest]"
        : [dest] "=r" (reg) // Outputs
        : // Inputs
        : // Clobbers
    );
    reg |= 1 << 5;
    asm volatile("mov %[dest], %%cr0"
         : // Outputs
         : [dest] "r" (reg) // Inputs
         : // Clobbers
    );


    asm volatile("mov %%cr4, %[dest]"
        : [dest] "=r" (reg) // Outputs
        : // Inputs
        : // Clobbers
    );
    reg |= 1 << 10;
    asm volatile("mov %[dest], %%cr4"
             : // Outputs
             : [dest] "r" (reg) // Inputs
             : // Clobbers
    );
    


    DT_STRUCT dgtData = {0};
    dgtData.Limit = sizeof(MinimalGDT) - 1;
    dgtData.BaseAddress = (uint64_t)MinimalGDT; //Take the address from the EfiLoaderData



    ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress1 = (uint16_t)((uint64_t)&tss64);
    ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress2 = (uint8_t)((uint64_t)&tss64 >> 16);
    ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress3 = (uint8_t)((uint64_t)&tss64 >> 24);
    ( (TSS_LDT_ENTRY_STRUCT*) &((GDT_ENTRY_STRUCT*)MinimalGDT)[3] )->BaseAddress4 = (uint32_t)((uint64_t)&tss64 >> 32); // TSS is a double-sized entry

    asm volatile("lgdt %[src]"
        : // Outputs
        : [src] "m" (dgtData) // Inputs
        : // Clobbers
    );

    uint16_t reg2 = 0x18;
    asm volatile("ltr %[src]"
        : // Outputs
        : [src] "m" (reg2) // Inputs
        : // Clobbers
    );

    asm volatile("mov $16, %ax \n\t" // Data segment index
               "mov %ax, %ds \n\t"
               "mov %ax, %es \n\t"
               "mov %ax, %fs \n\t"
               "mov %ax, %gs \n\t"
               "mov %ax, %ss \n\t"
               "movq $8, %rdx \n\t" // 64-bit code segment index
               // Store RIP offSet, pointing to right after 'lretq'
               "leaq 4(%rip), %rax \n\t" // This is hardcoded to the size of the rest of this little ASM block. %rip points to the next instruction, +4 bytes puts it right after 'lretq'
               "pushq %rdx \n\t"
               "pushq %rax \n\t"
               "lretq \n\t" // NOTE: lretq and retfq are the same. lretq is supported by both GCC and Clang, while retfq works only with GCC. Either way, opcode is 0x48CB.
               // The address loaded into %rax points here (right after 'lretq'), so execution returns to this point without breaking compiler compatibility
    );


    DT_STRUCT idtEntry = {0};

    idtEntry.Limit = sizeof(IDT_data) - 1;
    idtEntry.BaseAddress = (uint64_t)IDT_data;

    *(uint64_t*)(&tss64.IST1) = (uint64_t)(NMI_stack + (1 << 12));
    *(uint64_t*)(&tss64.IST2) = (uint64_t)(DF_stack + (1 << 12));
    *(uint64_t*)(&tss64.IST3) = (uint64_t)(MC_stack + (1 << 12));
    *(uint64_t*)(&tss64.IST4) = (uint64_t)(BP_stack + (1 << 12));

    //
    // Predefined System Interrupts and Exceptions
    //

    SetInterruptEntry(0, (uint64_t)DE_ISR_pusher0); // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
    SetInterruptEntry(1, (uint64_t)DB_ISR_pusher1); // Fault/Trap #DB: Debug Exception
    SetNMIInterruptEntry(2, (uint64_t)NMI_ISR_pusher2); // NMI (Nonmaskable External Interrupt)
    // Fun fact: Hyper-V will send a watchdog timeout via an NMI if the system is halted for a while. Looks like it's supposed to crash the VM via
    // triple fault if there's no handler Set up. Hpyer-V-Worker logs that the VM "has encountered a watchdog timeout and was reSet" in the Windows
    // event viewer when the VM receives the NMI. Neat.
    SetBPInterruptEntry(3, (uint64_t)BP_ISR_pusher3); // Trap #BP: Breakpoint (INT3 instruction)
    SetInterruptEntry(4, (uint64_t)OF_ISR_pusher4); // Trap #OF: Overflow (INTO instruction)
    SetInterruptEntry(5, (uint64_t)BR_ISR_pusher5); // Fault #BR: BOUND Range Exceeded (BOUND instruction)
    SetInterruptEntry(6, (uint64_t)UD_ISR_pusher6); // Fault #UD: Invalid or Undefined Opcode
    SetInterruptEntry(7, (uint64_t)NM_ISR_pusher7); // Fault #NM: Device Not Available Exception

    SetMCInterruptEntry(8, (uint64_t)DF_EXC_pusher8); // Abort #DF: Double Fault (error code is always 0)

    SetInterruptEntry(9, (uint64_t)CSO_ISR_pusher9); // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

    SetInterruptEntry(10, (uint64_t)TS_EXC_pusher10); // Fault #TS: Invalid TSS
    SetInterruptEntry(11, (uint64_t)NP_EXC_pusher11); // Fault #NP: Segment Not Present
    SetInterruptEntry(12, (uint64_t)SS_EXC_pusher12); // Fault #SS: Stack Segment Fault
    SetInterruptEntry(13, (uint64_t)GP_EXC_pusher13); // Fault #GP: General Protection
    SetInterruptEntry(14, (uint64_t)PF_EXC_pusher14); // Fault #PF: Page Fault

    SetInterruptEntry(16, (uint64_t)MF_ISR_pusher16); // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

    SetInterruptEntry(17, (uint64_t)AC_EXC_pusher17); // Fault #AC: Alignment Check (error code is always 0)

    SetMCInterruptEntry(18, (uint64_t)MC_ISR_pusher18); // Abort #MC: Machine Check
    SetInterruptEntry(19, (uint64_t)XM_ISR_pusher19); // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
    SetInterruptEntry(20, (uint64_t)VE_ISR_pusher20); // Fault #VE: Virtualization Exception

    SetInterruptEntry(30, (uint64_t)SX_EXC_pusher30); // Fault #SX: Security Exception

    //
    // These are system reserved, if they trigger they will go to unhandled interrupt error
    //

    SetInterruptEntry(15, (uint64_t)CPU_ISR_pusher15);

    SetInterruptEntry(21, (uint64_t)CPU_ISR_pusher21);
    SetInterruptEntry(22, (uint64_t)CPU_ISR_pusher22);
    SetInterruptEntry(23, (uint64_t)CPU_ISR_pusher23);
    SetInterruptEntry(24, (uint64_t)CPU_ISR_pusher24);
    SetInterruptEntry(25, (uint64_t)CPU_ISR_pusher25);
    SetInterruptEntry(26, (uint64_t)CPU_ISR_pusher26);
    SetInterruptEntry(27, (uint64_t)CPU_ISR_pusher27);
    SetInterruptEntry(28, (uint64_t)CPU_ISR_pusher28);
    SetInterruptEntry(29, (uint64_t)CPU_ISR_pusher29);

    SetInterruptEntry(31, (uint64_t)CPU_ISR_pusher31);

    //
    // User-Defined Interrupts
    //

    // By default everything is Set to USER_ISR_MACRO.

    SetInterruptEntry(32, (uint64_t)User_ISR_pusher32);
    SetInterruptEntry(33, (uint64_t)User_ISR_pusher33);
    SetInterruptEntry(34, (uint64_t)User_ISR_pusher34);
    SetInterruptEntry(35, (uint64_t)User_ISR_pusher35);
    SetInterruptEntry(36, (uint64_t)User_ISR_pusher36);
    SetInterruptEntry(37, (uint64_t)User_ISR_pusher37);
    SetInterruptEntry(38, (uint64_t)User_ISR_pusher38);
    SetInterruptEntry(39, (uint64_t)User_ISR_pusher39);
    SetInterruptEntry(40, (uint64_t)User_ISR_pusher40);
    SetInterruptEntry(41, (uint64_t)User_ISR_pusher41);
    SetInterruptEntry(42, (uint64_t)User_ISR_pusher42);
    SetInterruptEntry(43, (uint64_t)User_ISR_pusher43);
    SetInterruptEntry(44, (uint64_t)User_ISR_pusher44);
    SetInterruptEntry(45, (uint64_t)User_ISR_pusher45);
    SetInterruptEntry(46, (uint64_t)User_ISR_pusher46);
    SetInterruptEntry(47, (uint64_t)User_ISR_pusher47);
    SetInterruptEntry(48, (uint64_t)User_ISR_pusher48);
    SetInterruptEntry(49, (uint64_t)User_ISR_pusher49);
    SetInterruptEntry(50, (uint64_t)User_ISR_pusher50);
    SetInterruptEntry(51, (uint64_t)User_ISR_pusher51);
    SetInterruptEntry(52, (uint64_t)User_ISR_pusher52);
    SetInterruptEntry(53, (uint64_t)User_ISR_pusher53);
    SetInterruptEntry(54, (uint64_t)User_ISR_pusher54);
    SetInterruptEntry(55, (uint64_t)User_ISR_pusher55);
    SetInterruptEntry(56, (uint64_t)User_ISR_pusher56);
    SetInterruptEntry(57, (uint64_t)User_ISR_pusher57);
    SetInterruptEntry(58, (uint64_t)User_ISR_pusher58);
    SetInterruptEntry(59, (uint64_t)User_ISR_pusher59);
    SetInterruptEntry(60, (uint64_t)User_ISR_pusher60);
    SetInterruptEntry(61, (uint64_t)User_ISR_pusher61);
    SetInterruptEntry(62, (uint64_t)User_ISR_pusher62);
    SetInterruptEntry(63, (uint64_t)User_ISR_pusher63);
    SetInterruptEntry(64, (uint64_t)User_ISR_pusher64);
    SetInterruptEntry(65, (uint64_t)User_ISR_pusher65);
    SetInterruptEntry(66, (uint64_t)User_ISR_pusher66);
    SetInterruptEntry(67, (uint64_t)User_ISR_pusher67);
    SetInterruptEntry(68, (uint64_t)User_ISR_pusher68);
    SetInterruptEntry(69, (uint64_t)User_ISR_pusher69);
    SetInterruptEntry(70, (uint64_t)User_ISR_pusher70);
    SetInterruptEntry(71, (uint64_t)User_ISR_pusher71);
    SetInterruptEntry(72, (uint64_t)User_ISR_pusher72);
    SetInterruptEntry(73, (uint64_t)User_ISR_pusher73);
    SetInterruptEntry(74, (uint64_t)User_ISR_pusher74);
    SetInterruptEntry(75, (uint64_t)User_ISR_pusher75);
    SetInterruptEntry(76, (uint64_t)User_ISR_pusher76);
    SetInterruptEntry(77, (uint64_t)User_ISR_pusher77);
    SetInterruptEntry(78, (uint64_t)User_ISR_pusher78);
    SetInterruptEntry(79, (uint64_t)User_ISR_pusher79);
    SetInterruptEntry(80, (uint64_t)User_ISR_pusher80);
    SetInterruptEntry(81, (uint64_t)User_ISR_pusher81);
    SetInterruptEntry(82, (uint64_t)User_ISR_pusher82);
    SetInterruptEntry(83, (uint64_t)User_ISR_pusher83);
    SetInterruptEntry(84, (uint64_t)User_ISR_pusher84);
    SetInterruptEntry(85, (uint64_t)User_ISR_pusher85);
    SetInterruptEntry(86, (uint64_t)User_ISR_pusher86);
    SetInterruptEntry(87, (uint64_t)User_ISR_pusher87);
    SetInterruptEntry(88, (uint64_t)User_ISR_pusher88);
    SetInterruptEntry(89, (uint64_t)User_ISR_pusher89);
    SetInterruptEntry(90, (uint64_t)User_ISR_pusher90);
    SetInterruptEntry(91, (uint64_t)User_ISR_pusher91);
    SetInterruptEntry(92, (uint64_t)User_ISR_pusher92);
    SetInterruptEntry(93, (uint64_t)User_ISR_pusher93);
    SetInterruptEntry(94, (uint64_t)User_ISR_pusher94);
    SetInterruptEntry(95, (uint64_t)User_ISR_pusher95);
    SetInterruptEntry(96, (uint64_t)User_ISR_pusher96);
    SetInterruptEntry(97, (uint64_t)User_ISR_pusher97);
    SetInterruptEntry(98, (uint64_t)User_ISR_pusher98);
    SetInterruptEntry(99, (uint64_t)User_ISR_pusher99);
    SetInterruptEntry(100, (uint64_t)User_ISR_pusher100);
    SetInterruptEntry(101, (uint64_t)User_ISR_pusher101);
    SetInterruptEntry(102, (uint64_t)User_ISR_pusher102);
    SetInterruptEntry(103, (uint64_t)User_ISR_pusher103);
    SetInterruptEntry(104, (uint64_t)User_ISR_pusher104);
    SetInterruptEntry(105, (uint64_t)User_ISR_pusher105);
    SetInterruptEntry(106, (uint64_t)User_ISR_pusher106);
    SetInterruptEntry(107, (uint64_t)User_ISR_pusher107);
    SetInterruptEntry(108, (uint64_t)User_ISR_pusher108);
    SetInterruptEntry(109, (uint64_t)User_ISR_pusher109);
    SetInterruptEntry(110, (uint64_t)User_ISR_pusher110);
    SetInterruptEntry(111, (uint64_t)User_ISR_pusher111);
    SetInterruptEntry(112, (uint64_t)User_ISR_pusher112);
    SetInterruptEntry(113, (uint64_t)User_ISR_pusher113);
    SetInterruptEntry(114, (uint64_t)User_ISR_pusher114);
    SetInterruptEntry(115, (uint64_t)User_ISR_pusher115);
    SetInterruptEntry(116, (uint64_t)User_ISR_pusher116);
    SetInterruptEntry(117, (uint64_t)User_ISR_pusher117);
    SetInterruptEntry(118, (uint64_t)User_ISR_pusher118);
    SetInterruptEntry(119, (uint64_t)User_ISR_pusher119);
    SetInterruptEntry(120, (uint64_t)User_ISR_pusher120);
    SetInterruptEntry(121, (uint64_t)User_ISR_pusher121);
    SetInterruptEntry(122, (uint64_t)User_ISR_pusher122);
    SetInterruptEntry(123, (uint64_t)User_ISR_pusher123);
    SetInterruptEntry(124, (uint64_t)User_ISR_pusher124);
    SetInterruptEntry(125, (uint64_t)User_ISR_pusher125);
    SetInterruptEntry(126, (uint64_t)User_ISR_pusher126);
    SetInterruptEntry(127, (uint64_t)User_ISR_pusher127);
    SetInterruptEntry(128, (uint64_t)User_ISR_pusher128);
    SetInterruptEntry(129, (uint64_t)User_ISR_pusher129);
    SetInterruptEntry(130, (uint64_t)User_ISR_pusher130);
    SetInterruptEntry(131, (uint64_t)User_ISR_pusher131);
    SetInterruptEntry(132, (uint64_t)User_ISR_pusher132);
    SetInterruptEntry(133, (uint64_t)User_ISR_pusher133);
    SetInterruptEntry(134, (uint64_t)User_ISR_pusher134);
    SetInterruptEntry(135, (uint64_t)User_ISR_pusher135);
    SetInterruptEntry(136, (uint64_t)User_ISR_pusher136);
    SetInterruptEntry(137, (uint64_t)User_ISR_pusher137);
    SetInterruptEntry(138, (uint64_t)User_ISR_pusher138);
    SetInterruptEntry(139, (uint64_t)User_ISR_pusher139);
    SetInterruptEntry(140, (uint64_t)User_ISR_pusher140);
    SetInterruptEntry(141, (uint64_t)User_ISR_pusher141);
    SetInterruptEntry(142, (uint64_t)User_ISR_pusher142);
    SetInterruptEntry(143, (uint64_t)User_ISR_pusher143);
    SetInterruptEntry(144, (uint64_t)User_ISR_pusher144);
    SetInterruptEntry(145, (uint64_t)User_ISR_pusher145);
    SetInterruptEntry(146, (uint64_t)User_ISR_pusher146);
    SetInterruptEntry(147, (uint64_t)User_ISR_pusher147);
    SetInterruptEntry(148, (uint64_t)User_ISR_pusher148);
    SetInterruptEntry(149, (uint64_t)User_ISR_pusher149);
    SetInterruptEntry(150, (uint64_t)User_ISR_pusher150);
    SetInterruptEntry(151, (uint64_t)User_ISR_pusher151);
    SetInterruptEntry(152, (uint64_t)User_ISR_pusher152);
    SetInterruptEntry(153, (uint64_t)User_ISR_pusher153);
    SetInterruptEntry(154, (uint64_t)User_ISR_pusher154);
    SetInterruptEntry(155, (uint64_t)User_ISR_pusher155);
    SetInterruptEntry(156, (uint64_t)User_ISR_pusher156);
    SetInterruptEntry(157, (uint64_t)User_ISR_pusher157);
    SetInterruptEntry(158, (uint64_t)User_ISR_pusher158);
    SetInterruptEntry(159, (uint64_t)User_ISR_pusher159);
    SetInterruptEntry(160, (uint64_t)User_ISR_pusher160);
    SetInterruptEntry(161, (uint64_t)User_ISR_pusher161);
    SetInterruptEntry(162, (uint64_t)User_ISR_pusher162);
    SetInterruptEntry(163, (uint64_t)User_ISR_pusher163);
    SetInterruptEntry(164, (uint64_t)User_ISR_pusher164);
    SetInterruptEntry(165, (uint64_t)User_ISR_pusher165);
    SetInterruptEntry(166, (uint64_t)User_ISR_pusher166);
    SetInterruptEntry(167, (uint64_t)User_ISR_pusher167);
    SetInterruptEntry(168, (uint64_t)User_ISR_pusher168);
    SetInterruptEntry(169, (uint64_t)User_ISR_pusher169);
    SetInterruptEntry(170, (uint64_t)User_ISR_pusher170);
    SetInterruptEntry(171, (uint64_t)User_ISR_pusher171);
    SetInterruptEntry(172, (uint64_t)User_ISR_pusher172);
    SetInterruptEntry(173, (uint64_t)User_ISR_pusher173);
    SetInterruptEntry(174, (uint64_t)User_ISR_pusher174);
    SetInterruptEntry(175, (uint64_t)User_ISR_pusher175);
    SetInterruptEntry(176, (uint64_t)User_ISR_pusher176);
    SetInterruptEntry(177, (uint64_t)User_ISR_pusher177);
    SetInterruptEntry(178, (uint64_t)User_ISR_pusher178);
    SetInterruptEntry(179, (uint64_t)User_ISR_pusher179);
    SetInterruptEntry(180, (uint64_t)User_ISR_pusher180);
    SetInterruptEntry(181, (uint64_t)User_ISR_pusher181);
    SetInterruptEntry(182, (uint64_t)User_ISR_pusher182);
    SetInterruptEntry(183, (uint64_t)User_ISR_pusher183);
    SetInterruptEntry(184, (uint64_t)User_ISR_pusher184);
    SetInterruptEntry(185, (uint64_t)User_ISR_pusher185);
    SetInterruptEntry(186, (uint64_t)User_ISR_pusher186);
    SetInterruptEntry(187, (uint64_t)User_ISR_pusher187);
    SetInterruptEntry(188, (uint64_t)User_ISR_pusher188);
    SetInterruptEntry(189, (uint64_t)User_ISR_pusher189);
    SetInterruptEntry(190, (uint64_t)User_ISR_pusher190);
    SetInterruptEntry(191, (uint64_t)User_ISR_pusher191);
    SetInterruptEntry(192, (uint64_t)User_ISR_pusher192);
    SetInterruptEntry(193, (uint64_t)User_ISR_pusher193);
    SetInterruptEntry(194, (uint64_t)User_ISR_pusher194);
    SetInterruptEntry(195, (uint64_t)User_ISR_pusher195);
    SetInterruptEntry(196, (uint64_t)User_ISR_pusher196);
    SetInterruptEntry(197, (uint64_t)User_ISR_pusher197);
    SetInterruptEntry(198, (uint64_t)User_ISR_pusher198);
    SetInterruptEntry(199, (uint64_t)User_ISR_pusher199);
    SetInterruptEntry(200, (uint64_t)User_ISR_pusher200);
    SetInterruptEntry(201, (uint64_t)User_ISR_pusher201);
    SetInterruptEntry(202, (uint64_t)User_ISR_pusher202);
    SetInterruptEntry(203, (uint64_t)User_ISR_pusher203);
    SetInterruptEntry(204, (uint64_t)User_ISR_pusher204);
    SetInterruptEntry(205, (uint64_t)User_ISR_pusher205);
    SetInterruptEntry(206, (uint64_t)User_ISR_pusher206);
    SetInterruptEntry(207, (uint64_t)User_ISR_pusher207);
    SetInterruptEntry(208, (uint64_t)User_ISR_pusher208);
    SetInterruptEntry(209, (uint64_t)User_ISR_pusher209);
    SetInterruptEntry(210, (uint64_t)User_ISR_pusher210);
    SetInterruptEntry(211, (uint64_t)User_ISR_pusher211);
    SetInterruptEntry(212, (uint64_t)User_ISR_pusher212);
    SetInterruptEntry(213, (uint64_t)User_ISR_pusher213);
    SetInterruptEntry(214, (uint64_t)User_ISR_pusher214);
    SetInterruptEntry(215, (uint64_t)User_ISR_pusher215);
    SetInterruptEntry(216, (uint64_t)User_ISR_pusher216);
    SetInterruptEntry(217, (uint64_t)User_ISR_pusher217);
    SetInterruptEntry(218, (uint64_t)User_ISR_pusher218);
    SetInterruptEntry(219, (uint64_t)User_ISR_pusher219);
    SetInterruptEntry(220, (uint64_t)User_ISR_pusher220);
    SetInterruptEntry(221, (uint64_t)User_ISR_pusher221);
    SetInterruptEntry(222, (uint64_t)User_ISR_pusher222);
    SetInterruptEntry(223, (uint64_t)User_ISR_pusher223);
    SetInterruptEntry(224, (uint64_t)User_ISR_pusher224);
    SetInterruptEntry(225, (uint64_t)User_ISR_pusher225);
    SetInterruptEntry(226, (uint64_t)User_ISR_pusher226);
    SetInterruptEntry(227, (uint64_t)User_ISR_pusher227);
    SetInterruptEntry(228, (uint64_t)User_ISR_pusher228);
    SetInterruptEntry(229, (uint64_t)User_ISR_pusher229);
    SetInterruptEntry(230, (uint64_t)User_ISR_pusher230);
    SetInterruptEntry(231, (uint64_t)User_ISR_pusher231);
    SetInterruptEntry(232, (uint64_t)User_ISR_pusher232);
    SetInterruptEntry(233, (uint64_t)User_ISR_pusher233);
    SetInterruptEntry(234, (uint64_t)User_ISR_pusher234);
    SetInterruptEntry(235, (uint64_t)User_ISR_pusher235);
    SetInterruptEntry(236, (uint64_t)User_ISR_pusher236);
    SetInterruptEntry(237, (uint64_t)User_ISR_pusher237);
    SetInterruptEntry(238, (uint64_t)User_ISR_pusher238);
    SetInterruptEntry(239, (uint64_t)User_ISR_pusher239);
    SetInterruptEntry(240, (uint64_t)User_ISR_pusher240);
    SetInterruptEntry(241, (uint64_t)User_ISR_pusher241);
    SetInterruptEntry(242, (uint64_t)User_ISR_pusher242);
    SetInterruptEntry(243, (uint64_t)User_ISR_pusher243);
    SetInterruptEntry(244, (uint64_t)User_ISR_pusher244);
    SetInterruptEntry(245, (uint64_t)User_ISR_pusher245);
    SetInterruptEntry(246, (uint64_t)User_ISR_pusher246);
    SetInterruptEntry(247, (uint64_t)User_ISR_pusher247);
    SetInterruptEntry(248, (uint64_t)User_ISR_pusher248);
    SetInterruptEntry(249, (uint64_t)User_ISR_pusher249);
    SetInterruptEntry(250, (uint64_t)User_ISR_pusher250);
    SetInterruptEntry(251, (uint64_t)User_ISR_pusher251);
    SetInterruptEntry(252, (uint64_t)User_ISR_pusher252);
    SetInterruptEntry(253, (uint64_t)User_ISR_pusher253);
    SetInterruptEntry(254, (uint64_t)User_ISR_pusher254);
    SetInterruptEntry(255, (uint64_t)User_ISR_pusher255);



    asm volatile("lidt %[src]"
        : // Outputs
        : [src] "m" (idtEntry) // Inputs
        : // Clobbers
    );

}





static void SetInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
{
    IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 0x00;
    IDT_data[isrNum].Misc = 0x8E;
    IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
    IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
    IDT_data[isrNum].Reserved = 0;
}

static void SetTrapEntry(uint64_t isrNum, uint64_t isrAddr)
{
    IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 0x00;
    IDT_data[isrNum].Misc = 0x8F;
    IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
    IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
    IDT_data[isrNum].Reserved = 0;
}

static void SetUnusedEntry(uint64_t isrNum)
{
    IDT_data[isrNum].Offset1 = 0;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 0;
    IDT_data[isrNum].Misc = 0x0E;
    IDT_data[isrNum].Offset2 = 0;
    IDT_data[isrNum].Offset3 = 0;
    IDT_data[isrNum].Reserved = 0;
}
static void SetNMIInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
{

  IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
  IDT_data[isrNum].SegmentSelector = 0x08;
  IDT_data[isrNum].ISTandZero = 1;
  IDT_data[isrNum].Misc = 0x8E;
  IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
  IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
  IDT_data[isrNum].Reserved = 0;
}

// Double fault
static void SetDFInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
{
  IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
  IDT_data[isrNum].SegmentSelector = 0x08;
  IDT_data[isrNum].ISTandZero = 2;
  IDT_data[isrNum].Misc = 0x8E;
  IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
  IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
  IDT_data[isrNum].Reserved = 0;
}

// Machine Check
static void SetMCInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
{
    IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 3;
    IDT_data[isrNum].Misc = 0x8E;
    IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
    IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
    IDT_data[isrNum].Reserved = 0;
}

// Debug (INT3)
static void SetBPInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
{
    IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 4;
    IDT_data[isrNum].Misc = 0x8E;
    IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
    IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
    IDT_data[isrNum].Reserved = 0;
}


void User_ISR_handler(INTERRUPT_FRAME * i_frame)
{
    // %rdx: Mask for xcr0 [63:32], %rax: Mask for xcr0 [31:0]
    asm volatile ("xsave64 %[area]"
                : // No outputs
                : "a" (0xE7), "d" (0x00), [area] "m" (user_xsave_space) // Inputs
                : "memory" // Clobbers
              );

  // OK, since xsave has been called we can now safely use AVX instructions in this interrupt--up until xrstor is called, at any rate.
  // Using an interrupt gate in the IDT means we won't get preempted now, either, which would wreck the xsave area.

  switch(i_frame->isr_num)
  {

    //
    // User-Defined Interrupts (32-255)
    //

    //    case 32: // Minimum allowed user-defined case number
    //    // Case 32 code
    //      break;
    //    ....
    //    case 255: // Maximum allowed case number
    //    // Case 255 code
    //      break;

    //
    // End of User-Defined Interrupts
    //

    default:
        Abort(0xFFFFFFFFFFFFFFFF);
      break;
  }
}

void CPU_ISR_handler(INTERRUPT_FRAME * i_frame)
{
    Abort(0xFFFFFFFFFFFFFFFF);
}


//
// CPU Special Handlers
//

// Vector 0
void DE_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
{
    Abort(0);
}

// Vector 1
void DB_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault/Trap #DB: Debug Exception
{
    Abort(1);
}

// Vector 2
void NMI_ISR_handler(INTERRUPT_FRAME * i_frame) // NMI (Nonmaskable External Interrupt)
{
    Abort(2);
}

// Vector 3
void BP_ISR_handler(INTERRUPT_FRAME * i_frame) // Trap #BP: Breakpoint (INT3 instruction)
{
    Abort(3);
}

// Vector 4
void OF_ISR_handler(INTERRUPT_FRAME * i_frame) // Trap #OF: Overflow (INTO instruction)
{
    Abort(4);
}

// Vector 5
void BR_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault #BR: BOUND Range Exceeded (BOUND instruction)
{
    Abort(5);
}

// Vector 6
void UD_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault #UD: Invalid or Undefined Opcode
{
    Abort(6);
}

// Vector 7
void NM_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault #NM: Device Not Available Exception
{
    Abort(7);
}

// Vector 8
void DF_EXC_handler(EXCEPTION_FRAME * e_frame) // Abort #DF: Double Fault (error code is always 0)
{
    Abort(8);
}

// Vector 9
void CSO_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)
{
    Abort(9);
}

// Vector 10
void TS_EXC_handler(EXCEPTION_FRAME * e_frame) // Fault #TS: Invalid TSS
{
    Abort(10);
}

// Vector 11
void NP_EXC_handler(EXCEPTION_FRAME * e_frame) // Fault #NP: Segment Not Present
{
    Abort(11);
}

// Vector 12
void SS_EXC_handler(EXCEPTION_FRAME * e_frame) // Fault #SS: Stack Segment Fault
{
    Abort(12);
}

// Vector 13
void GP_EXC_handler(EXCEPTION_FRAME * e_frame) // Fault #GP: General Protection
{
    Abort(13);
}

// Vector 14
void PF_EXC_handler(EXCEPTION_FRAME * e_frame) // Fault #PF: Page Fault
{
    Abort(14);
}

// Vector 15 (Reserved)


// Vector 16
void MF_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)
{
    Abort(16);
}

// Vector 17
void AC_EXC_handler(EXCEPTION_FRAME * e_frame) // Fault #AC: Alignment Check (error code is always 0)
{
    Abort(17);
}

// Vector 18
void MC_ISR_handler(INTERRUPT_FRAME * i_frame) // Abort #MC: Machine Check
{
    Abort(18);
}

// Vector 19
void XM_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
{
    Abort(19);
}

// Vector 20
void VE_ISR_handler(INTERRUPT_FRAME * i_frame) // Fault #VE: Virtualization Exception
{
    Abort(20);
}

// Vectors 21-29 (Reserved)

// Vector 30
void SX_EXC_handler(EXCEPTION_FRAME * e_frame) // Fault #SX: Security Exception
{
    Abort(30);
}