#include "kernel/kernel.h"
#include "ISR.h"
#include "system.h"
//#include "kernel/EfiTypes.h"
//#include "kernel/EfiBind.h"
//#include "kernel/EfiErr.h"


static void setInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void setNMIInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void setBPInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void setMCInterruptEntry(uint64_t isrNum, uint64_t isrAddr);
static void setTrapEntry(uint64_t isrNum, uint64_t isrAddr);

__attribute__((aligned(64))) TSS64_STRUCT tss64 = {0};

__attribute__((aligned(64))) static IDT_GATE_STRUCT IDT_data[256] = {0};
__attribute__((aligned(64))) static volatile unsigned char NMI_stack[1 << 12] = {0};
__attribute__((aligned(64))) static volatile unsigned char DF_stack[1 << 12] = {0};
__attribute__((aligned(64))) static volatile unsigned char MC_stack[1 << 12] = {0};
__attribute__((aligned(64))) static volatile unsigned char BP_stack[1 << 12] = {0};

#define XSAVE_SIZE (1 << 13)

__attribute__((aligned(64))) static volatile unsigned char cpu_xsave_space[XSAVE_SIZE] = {0}; // Generic space for unhandled/unknown IDT vectors in the 0-31 range.
__attribute__((aligned(64))) static volatile unsigned char user_xsave_space[XSAVE_SIZE] = {0}; // For vectors 32-255, which can't preempt each other due to interrupt gating (IF in RFLAGS is cleared during ISR execution)


void Initialize_ISR()
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
               // Store RIP offset, pointing to right after 'lretq'
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

    setInterruptEntry(0, (uint64_t)DE_ISR_pusher0); // Fault #DE: Divide Error (divide by 0 or not enough bits in destination)
    setInterruptEntry(1, (uint64_t)DB_ISR_pusher1); // Fault/Trap #DB: Debug Exception
    setNMIInterruptEntry(2, (uint64_t)NMI_ISR_pusher2); // NMI (Nonmaskable External Interrupt)
    // Fun fact: Hyper-V will send a watchdog timeout via an NMI if the system is halted for a while. Looks like it's supposed to crash the VM via
    // triple fault if there's no handler set up. Hpyer-V-Worker logs that the VM "has encountered a watchdog timeout and was reset" in the Windows
    // event viewer when the VM receives the NMI. Neat.
    setBPInterruptEntry(3, (uint64_t)BP_ISR_pusher3); // Trap #BP: Breakpoint (INT3 instruction)
    setInterruptEntry(4, (uint64_t)OF_ISR_pusher4); // Trap #OF: Overflow (INTO instruction)
    setInterruptEntry(5, (uint64_t)BR_ISR_pusher5); // Fault #BR: BOUND Range Exceeded (BOUND instruction)
    setInterruptEntry(6, (uint64_t)UD_ISR_pusher6); // Fault #UD: Invalid or Undefined Opcode
    setInterruptEntry(7, (uint64_t)NM_ISR_pusher7); // Fault #NM: Device Not Available Exception

    setMCInterruptEntry(8, (uint64_t)DF_EXC_pusher8); // Abort #DF: Double Fault (error code is always 0)

    setInterruptEntry(9, (uint64_t)CSO_ISR_pusher9); // Fault (i386): Coprocessor Segment Overrun (long since obsolete, included for completeness)

    setInterruptEntry(10, (uint64_t)TS_EXC_pusher10); // Fault #TS: Invalid TSS
    setInterruptEntry(11, (uint64_t)NP_EXC_pusher11); // Fault #NP: Segment Not Present
    setInterruptEntry(12, (uint64_t)SS_EXC_pusher12); // Fault #SS: Stack Segment Fault
    setInterruptEntry(13, (uint64_t)GP_EXC_pusher13); // Fault #GP: General Protection
    setInterruptEntry(14, (uint64_t)PF_EXC_pusher14); // Fault #PF: Page Fault

    setInterruptEntry(16, (uint64_t)MF_ISR_pusher16); // Fault #MF: Math Error (x87 FPU Floating-Point Math Error)

    setInterruptEntry(17, (uint64_t)AC_EXC_pusher17); // Fault #AC: Alignment Check (error code is always 0)

    setMCInterruptEntry(18, (uint64_t)MC_ISR_pusher18); // Abort #MC: Machine Check
    setInterruptEntry(19, (uint64_t)XM_ISR_pusher19); // Fault #XM: SIMD Floating-Point Exception (SSE instructions)
    setInterruptEntry(20, (uint64_t)VE_ISR_pusher20); // Fault #VE: Virtualization Exception

    setInterruptEntry(30, (uint64_t)SX_EXC_pusher30); // Fault #SX: Security Exception

    //
    // These are system reserved, if they trigger they will go to unhandled interrupt error
    //

    setInterruptEntry(15, (uint64_t)CPU_ISR_pusher15);

    setInterruptEntry(21, (uint64_t)CPU_ISR_pusher21);
    setInterruptEntry(22, (uint64_t)CPU_ISR_pusher22);
    setInterruptEntry(23, (uint64_t)CPU_ISR_pusher23);
    setInterruptEntry(24, (uint64_t)CPU_ISR_pusher24);
    setInterruptEntry(25, (uint64_t)CPU_ISR_pusher25);
    setInterruptEntry(26, (uint64_t)CPU_ISR_pusher26);
    setInterruptEntry(27, (uint64_t)CPU_ISR_pusher27);
    setInterruptEntry(28, (uint64_t)CPU_ISR_pusher28);
    setInterruptEntry(29, (uint64_t)CPU_ISR_pusher29);

    setInterruptEntry(31, (uint64_t)CPU_ISR_pusher31);

    //
    // User-Defined Interrupts
    //

    // By default everything is set to USER_ISR_MACRO.

    setInterruptEntry(32, (uint64_t)User_ISR_pusher32);
    setInterruptEntry(33, (uint64_t)User_ISR_pusher33);
    setInterruptEntry(34, (uint64_t)User_ISR_pusher34);
    setInterruptEntry(35, (uint64_t)User_ISR_pusher35);
    setInterruptEntry(36, (uint64_t)User_ISR_pusher36);
    setInterruptEntry(37, (uint64_t)User_ISR_pusher37);
    setInterruptEntry(38, (uint64_t)User_ISR_pusher38);
    setInterruptEntry(39, (uint64_t)User_ISR_pusher39);
    setInterruptEntry(40, (uint64_t)User_ISR_pusher40);
    setInterruptEntry(41, (uint64_t)User_ISR_pusher41);
    setInterruptEntry(42, (uint64_t)User_ISR_pusher42);
    setInterruptEntry(43, (uint64_t)User_ISR_pusher43);
    setInterruptEntry(44, (uint64_t)User_ISR_pusher44);
    setInterruptEntry(45, (uint64_t)User_ISR_pusher45);
    setInterruptEntry(46, (uint64_t)User_ISR_pusher46);
    setInterruptEntry(47, (uint64_t)User_ISR_pusher47);
    setInterruptEntry(48, (uint64_t)User_ISR_pusher48);
    setInterruptEntry(49, (uint64_t)User_ISR_pusher49);
    setInterruptEntry(50, (uint64_t)User_ISR_pusher50);
    setInterruptEntry(51, (uint64_t)User_ISR_pusher51);
    setInterruptEntry(52, (uint64_t)User_ISR_pusher52);
    setInterruptEntry(53, (uint64_t)User_ISR_pusher53);
    setInterruptEntry(54, (uint64_t)User_ISR_pusher54);
    setInterruptEntry(55, (uint64_t)User_ISR_pusher55);
    setInterruptEntry(56, (uint64_t)User_ISR_pusher56);
    setInterruptEntry(57, (uint64_t)User_ISR_pusher57);
    setInterruptEntry(58, (uint64_t)User_ISR_pusher58);
    setInterruptEntry(59, (uint64_t)User_ISR_pusher59);
    setInterruptEntry(60, (uint64_t)User_ISR_pusher60);
    setInterruptEntry(61, (uint64_t)User_ISR_pusher61);
    setInterruptEntry(62, (uint64_t)User_ISR_pusher62);
    setInterruptEntry(63, (uint64_t)User_ISR_pusher63);
    setInterruptEntry(64, (uint64_t)User_ISR_pusher64);
    setInterruptEntry(65, (uint64_t)User_ISR_pusher65);
    setInterruptEntry(66, (uint64_t)User_ISR_pusher66);
    setInterruptEntry(67, (uint64_t)User_ISR_pusher67);
    setInterruptEntry(68, (uint64_t)User_ISR_pusher68);
    setInterruptEntry(69, (uint64_t)User_ISR_pusher69);
    setInterruptEntry(70, (uint64_t)User_ISR_pusher70);
    setInterruptEntry(71, (uint64_t)User_ISR_pusher71);
    setInterruptEntry(72, (uint64_t)User_ISR_pusher72);
    setInterruptEntry(73, (uint64_t)User_ISR_pusher73);
    setInterruptEntry(74, (uint64_t)User_ISR_pusher74);
    setInterruptEntry(75, (uint64_t)User_ISR_pusher75);
    setInterruptEntry(76, (uint64_t)User_ISR_pusher76);
    setInterruptEntry(77, (uint64_t)User_ISR_pusher77);
    setInterruptEntry(78, (uint64_t)User_ISR_pusher78);
    setInterruptEntry(79, (uint64_t)User_ISR_pusher79);
    setInterruptEntry(80, (uint64_t)User_ISR_pusher80);
    setInterruptEntry(81, (uint64_t)User_ISR_pusher81);
    setInterruptEntry(82, (uint64_t)User_ISR_pusher82);
    setInterruptEntry(83, (uint64_t)User_ISR_pusher83);
    setInterruptEntry(84, (uint64_t)User_ISR_pusher84);
    setInterruptEntry(85, (uint64_t)User_ISR_pusher85);
    setInterruptEntry(86, (uint64_t)User_ISR_pusher86);
    setInterruptEntry(87, (uint64_t)User_ISR_pusher87);
    setInterruptEntry(88, (uint64_t)User_ISR_pusher88);
    setInterruptEntry(89, (uint64_t)User_ISR_pusher89);
    setInterruptEntry(90, (uint64_t)User_ISR_pusher90);
    setInterruptEntry(91, (uint64_t)User_ISR_pusher91);
    setInterruptEntry(92, (uint64_t)User_ISR_pusher92);
    setInterruptEntry(93, (uint64_t)User_ISR_pusher93);
    setInterruptEntry(94, (uint64_t)User_ISR_pusher94);
    setInterruptEntry(95, (uint64_t)User_ISR_pusher95);
    setInterruptEntry(96, (uint64_t)User_ISR_pusher96);
    setInterruptEntry(97, (uint64_t)User_ISR_pusher97);
    setInterruptEntry(98, (uint64_t)User_ISR_pusher98);
    setInterruptEntry(99, (uint64_t)User_ISR_pusher99);
    setInterruptEntry(100, (uint64_t)User_ISR_pusher100);
    setInterruptEntry(101, (uint64_t)User_ISR_pusher101);
    setInterruptEntry(102, (uint64_t)User_ISR_pusher102);
    setInterruptEntry(103, (uint64_t)User_ISR_pusher103);
    setInterruptEntry(104, (uint64_t)User_ISR_pusher104);
    setInterruptEntry(105, (uint64_t)User_ISR_pusher105);
    setInterruptEntry(106, (uint64_t)User_ISR_pusher106);
    setInterruptEntry(107, (uint64_t)User_ISR_pusher107);
    setInterruptEntry(108, (uint64_t)User_ISR_pusher108);
    setInterruptEntry(109, (uint64_t)User_ISR_pusher109);
    setInterruptEntry(110, (uint64_t)User_ISR_pusher110);
    setInterruptEntry(111, (uint64_t)User_ISR_pusher111);
    setInterruptEntry(112, (uint64_t)User_ISR_pusher112);
    setInterruptEntry(113, (uint64_t)User_ISR_pusher113);
    setInterruptEntry(114, (uint64_t)User_ISR_pusher114);
    setInterruptEntry(115, (uint64_t)User_ISR_pusher115);
    setInterruptEntry(116, (uint64_t)User_ISR_pusher116);
    setInterruptEntry(117, (uint64_t)User_ISR_pusher117);
    setInterruptEntry(118, (uint64_t)User_ISR_pusher118);
    setInterruptEntry(119, (uint64_t)User_ISR_pusher119);
    setInterruptEntry(120, (uint64_t)User_ISR_pusher120);
    setInterruptEntry(121, (uint64_t)User_ISR_pusher121);
    setInterruptEntry(122, (uint64_t)User_ISR_pusher122);
    setInterruptEntry(123, (uint64_t)User_ISR_pusher123);
    setInterruptEntry(124, (uint64_t)User_ISR_pusher124);
    setInterruptEntry(125, (uint64_t)User_ISR_pusher125);
    setInterruptEntry(126, (uint64_t)User_ISR_pusher126);
    setInterruptEntry(127, (uint64_t)User_ISR_pusher127);
    setInterruptEntry(128, (uint64_t)User_ISR_pusher128);
    setInterruptEntry(129, (uint64_t)User_ISR_pusher129);
    setInterruptEntry(130, (uint64_t)User_ISR_pusher130);
    setInterruptEntry(131, (uint64_t)User_ISR_pusher131);
    setInterruptEntry(132, (uint64_t)User_ISR_pusher132);
    setInterruptEntry(133, (uint64_t)User_ISR_pusher133);
    setInterruptEntry(134, (uint64_t)User_ISR_pusher134);
    setInterruptEntry(135, (uint64_t)User_ISR_pusher135);
    setInterruptEntry(136, (uint64_t)User_ISR_pusher136);
    setInterruptEntry(137, (uint64_t)User_ISR_pusher137);
    setInterruptEntry(138, (uint64_t)User_ISR_pusher138);
    setInterruptEntry(139, (uint64_t)User_ISR_pusher139);
    setInterruptEntry(140, (uint64_t)User_ISR_pusher140);
    setInterruptEntry(141, (uint64_t)User_ISR_pusher141);
    setInterruptEntry(142, (uint64_t)User_ISR_pusher142);
    setInterruptEntry(143, (uint64_t)User_ISR_pusher143);
    setInterruptEntry(144, (uint64_t)User_ISR_pusher144);
    setInterruptEntry(145, (uint64_t)User_ISR_pusher145);
    setInterruptEntry(146, (uint64_t)User_ISR_pusher146);
    setInterruptEntry(147, (uint64_t)User_ISR_pusher147);
    setInterruptEntry(148, (uint64_t)User_ISR_pusher148);
    setInterruptEntry(149, (uint64_t)User_ISR_pusher149);
    setInterruptEntry(150, (uint64_t)User_ISR_pusher150);
    setInterruptEntry(151, (uint64_t)User_ISR_pusher151);
    setInterruptEntry(152, (uint64_t)User_ISR_pusher152);
    setInterruptEntry(153, (uint64_t)User_ISR_pusher153);
    setInterruptEntry(154, (uint64_t)User_ISR_pusher154);
    setInterruptEntry(155, (uint64_t)User_ISR_pusher155);
    setInterruptEntry(156, (uint64_t)User_ISR_pusher156);
    setInterruptEntry(157, (uint64_t)User_ISR_pusher157);
    setInterruptEntry(158, (uint64_t)User_ISR_pusher158);
    setInterruptEntry(159, (uint64_t)User_ISR_pusher159);
    setInterruptEntry(160, (uint64_t)User_ISR_pusher160);
    setInterruptEntry(161, (uint64_t)User_ISR_pusher161);
    setInterruptEntry(162, (uint64_t)User_ISR_pusher162);
    setInterruptEntry(163, (uint64_t)User_ISR_pusher163);
    setInterruptEntry(164, (uint64_t)User_ISR_pusher164);
    setInterruptEntry(165, (uint64_t)User_ISR_pusher165);
    setInterruptEntry(166, (uint64_t)User_ISR_pusher166);
    setInterruptEntry(167, (uint64_t)User_ISR_pusher167);
    setInterruptEntry(168, (uint64_t)User_ISR_pusher168);
    setInterruptEntry(169, (uint64_t)User_ISR_pusher169);
    setInterruptEntry(170, (uint64_t)User_ISR_pusher170);
    setInterruptEntry(171, (uint64_t)User_ISR_pusher171);
    setInterruptEntry(172, (uint64_t)User_ISR_pusher172);
    setInterruptEntry(173, (uint64_t)User_ISR_pusher173);
    setInterruptEntry(174, (uint64_t)User_ISR_pusher174);
    setInterruptEntry(175, (uint64_t)User_ISR_pusher175);
    setInterruptEntry(176, (uint64_t)User_ISR_pusher176);
    setInterruptEntry(177, (uint64_t)User_ISR_pusher177);
    setInterruptEntry(178, (uint64_t)User_ISR_pusher178);
    setInterruptEntry(179, (uint64_t)User_ISR_pusher179);
    setInterruptEntry(180, (uint64_t)User_ISR_pusher180);
    setInterruptEntry(181, (uint64_t)User_ISR_pusher181);
    setInterruptEntry(182, (uint64_t)User_ISR_pusher182);
    setInterruptEntry(183, (uint64_t)User_ISR_pusher183);
    setInterruptEntry(184, (uint64_t)User_ISR_pusher184);
    setInterruptEntry(185, (uint64_t)User_ISR_pusher185);
    setInterruptEntry(186, (uint64_t)User_ISR_pusher186);
    setInterruptEntry(187, (uint64_t)User_ISR_pusher187);
    setInterruptEntry(188, (uint64_t)User_ISR_pusher188);
    setInterruptEntry(189, (uint64_t)User_ISR_pusher189);
    setInterruptEntry(190, (uint64_t)User_ISR_pusher190);
    setInterruptEntry(191, (uint64_t)User_ISR_pusher191);
    setInterruptEntry(192, (uint64_t)User_ISR_pusher192);
    setInterruptEntry(193, (uint64_t)User_ISR_pusher193);
    setInterruptEntry(194, (uint64_t)User_ISR_pusher194);
    setInterruptEntry(195, (uint64_t)User_ISR_pusher195);
    setInterruptEntry(196, (uint64_t)User_ISR_pusher196);
    setInterruptEntry(197, (uint64_t)User_ISR_pusher197);
    setInterruptEntry(198, (uint64_t)User_ISR_pusher198);
    setInterruptEntry(199, (uint64_t)User_ISR_pusher199);
    setInterruptEntry(200, (uint64_t)User_ISR_pusher200);
    setInterruptEntry(201, (uint64_t)User_ISR_pusher201);
    setInterruptEntry(202, (uint64_t)User_ISR_pusher202);
    setInterruptEntry(203, (uint64_t)User_ISR_pusher203);
    setInterruptEntry(204, (uint64_t)User_ISR_pusher204);
    setInterruptEntry(205, (uint64_t)User_ISR_pusher205);
    setInterruptEntry(206, (uint64_t)User_ISR_pusher206);
    setInterruptEntry(207, (uint64_t)User_ISR_pusher207);
    setInterruptEntry(208, (uint64_t)User_ISR_pusher208);
    setInterruptEntry(209, (uint64_t)User_ISR_pusher209);
    setInterruptEntry(210, (uint64_t)User_ISR_pusher210);
    setInterruptEntry(211, (uint64_t)User_ISR_pusher211);
    setInterruptEntry(212, (uint64_t)User_ISR_pusher212);
    setInterruptEntry(213, (uint64_t)User_ISR_pusher213);
    setInterruptEntry(214, (uint64_t)User_ISR_pusher214);
    setInterruptEntry(215, (uint64_t)User_ISR_pusher215);
    setInterruptEntry(216, (uint64_t)User_ISR_pusher216);
    setInterruptEntry(217, (uint64_t)User_ISR_pusher217);
    setInterruptEntry(218, (uint64_t)User_ISR_pusher218);
    setInterruptEntry(219, (uint64_t)User_ISR_pusher219);
    setInterruptEntry(220, (uint64_t)User_ISR_pusher220);
    setInterruptEntry(221, (uint64_t)User_ISR_pusher221);
    setInterruptEntry(222, (uint64_t)User_ISR_pusher222);
    setInterruptEntry(223, (uint64_t)User_ISR_pusher223);
    setInterruptEntry(224, (uint64_t)User_ISR_pusher224);
    setInterruptEntry(225, (uint64_t)User_ISR_pusher225);
    setInterruptEntry(226, (uint64_t)User_ISR_pusher226);
    setInterruptEntry(227, (uint64_t)User_ISR_pusher227);
    setInterruptEntry(228, (uint64_t)User_ISR_pusher228);
    setInterruptEntry(229, (uint64_t)User_ISR_pusher229);
    setInterruptEntry(230, (uint64_t)User_ISR_pusher230);
    setInterruptEntry(231, (uint64_t)User_ISR_pusher231);
    setInterruptEntry(232, (uint64_t)User_ISR_pusher232);
    setInterruptEntry(233, (uint64_t)User_ISR_pusher233);
    setInterruptEntry(234, (uint64_t)User_ISR_pusher234);
    setInterruptEntry(235, (uint64_t)User_ISR_pusher235);
    setInterruptEntry(236, (uint64_t)User_ISR_pusher236);
    setInterruptEntry(237, (uint64_t)User_ISR_pusher237);
    setInterruptEntry(238, (uint64_t)User_ISR_pusher238);
    setInterruptEntry(239, (uint64_t)User_ISR_pusher239);
    setInterruptEntry(240, (uint64_t)User_ISR_pusher240);
    setInterruptEntry(241, (uint64_t)User_ISR_pusher241);
    setInterruptEntry(242, (uint64_t)User_ISR_pusher242);
    setInterruptEntry(243, (uint64_t)User_ISR_pusher243);
    setInterruptEntry(244, (uint64_t)User_ISR_pusher244);
    setInterruptEntry(245, (uint64_t)User_ISR_pusher245);
    setInterruptEntry(246, (uint64_t)User_ISR_pusher246);
    setInterruptEntry(247, (uint64_t)User_ISR_pusher247);
    setInterruptEntry(248, (uint64_t)User_ISR_pusher248);
    setInterruptEntry(249, (uint64_t)User_ISR_pusher249);
    setInterruptEntry(250, (uint64_t)User_ISR_pusher250);
    setInterruptEntry(251, (uint64_t)User_ISR_pusher251);
    setInterruptEntry(252, (uint64_t)User_ISR_pusher252);
    setInterruptEntry(253, (uint64_t)User_ISR_pusher253);
    setInterruptEntry(254, (uint64_t)User_ISR_pusher254);
    setInterruptEntry(255, (uint64_t)User_ISR_pusher255);



    asm volatile("lidt %[src]"
        : // Outputs
        : [src] "m" (idtEntry) // Inputs
        : // Clobbers
    );

}





static void setInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
{
    IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 0x00;
    IDT_data[isrNum].Misc = 0x8E;
    IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
    IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
    IDT_data[isrNum].Reserved = 0;
}

static void setTrapEntry(uint64_t isrNum, uint64_t isrAddr)
{
    IDT_data[isrNum].Offset1 = (uint16_t)isrAddr;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 0x00;
    IDT_data[isrNum].Misc = 0x8F;
    IDT_data[isrNum].Offset2 = (uint16_t)(isrAddr >> 16);
    IDT_data[isrNum].Offset3 = (uint32_t)(isrAddr >> 32);
    IDT_data[isrNum].Reserved = 0;
}

static void setUnusedEntry(uint64_t isrNum)
{
    IDT_data[isrNum].Offset1 = 0;
    IDT_data[isrNum].SegmentSelector = 0x08;
    IDT_data[isrNum].ISTandZero = 0;
    IDT_data[isrNum].Misc = 0x0E;
    IDT_data[isrNum].Offset2 = 0;
    IDT_data[isrNum].Offset3 = 0;
    IDT_data[isrNum].Reserved = 0;
}
static void setNMIInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
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
static void setDFInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
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
static void setMCInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
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
static void setBPInterruptEntry(uint64_t isrNum, uint64_t isrAddr)
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