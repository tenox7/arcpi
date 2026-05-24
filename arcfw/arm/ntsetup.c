//
// ARM analog of BOOT/LIB/{MIPS,ALPHA}/NTSETUP.C - the per-architecture setup the
// OS loader does between loading the kernel image and transferring control to it
// (osloader.c:779 BlSetupForNt). Replaces the stubs.c placeholder.
//
// The MIPS/Alpha versions build kernel page tables and allocate the idle thread's
// kernel stack, panic stack, PCR, and PDR pages. Our stand-in kernel runs MMU-off
// (no page tables yet - KSEG0_BASE == 0, bldr.h) and only a "hello world" kernel,
// so the one thing it genuinely needs is a stack: KE/MIPS/X4START.S loads sp from
// LoaderBlock->KernelStack, and so does our start.S. We allocate that and nothing
// more; PCR/PDR/panic-stack/page-tables arrive with a real kernel.
//
#include "bldr.h"

//
// The kernel's HAL console (arcfw/kernel/jxdisp.c) needs the OEM font + the VideoCore
// framebuffer geometry via the loader block. Set here (in the shared arch setup) so BOTH
// boot paths get it: the [1] BlArmBootKernel shortcut AND the genuine BlOsLoader ([3]),
// which calls BlSetupForNt at osloader.c:779 but has no notion of our ARM framebuffer.
//
extern unsigned int *fb_base;
extern unsigned int fb_width, fb_height, fb_pitch, fb_order;
extern unsigned char _binary_font_fon_start[];

//
// Idle-thread kernel stack size. MIPS/Alpha use KERNEL_STACK_SIZE from the kernel
// headers (not in our shim set); 16 KiB (4 pages) matches their order of magnitude
// and is ample for the stand-in kernel.
//
#define KERNEL_STACK_SIZE 0x4000

ARC_STATUS
BlSetupForNt(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )
{
    ARC_STATUS Status;
    ULONG KernelPage;

    //
    // Allocate kernel stack pages for the boot processor's idle thread, exactly as
    // MIPS NTSETUP.C does. BasePage 0 means "anywhere free". KernelStack points at
    // the TOP of the region (high address) because the stack grows down; start.S
    // sets sp to it. With KSEG0_BASE == 0 (MMU off) this is a usable physical
    // address.
    //
    Status = BlAllocateDescriptor(LoaderStartupKernelStack,
                                  0,
                                  KERNEL_STACK_SIZE >> PAGE_SHIFT,
                                  &KernelPage);

    if (Status != ESUCCESS) {
        return Status;
    }

    LoaderBlock->KernelStack =
        (KSEG0_BASE | (KernelPage << PAGE_SHIFT)) + KERNEL_STACK_SIZE;

    //
    // Hand the kernel its console: the OEM font (OemFontFile, the same field the MIPS/
    // Alpha loader fills) and the VideoCore framebuffer geometry (the ARM_LOADER_BLOCK
    // fields). jxdisp.c's HalpInitializeDisplay0 reads these; without them it falls back
    // to serial-only.
    //
    LoaderBlock->OemFontFile = (PVOID)_binary_font_fon_start;
    LoaderBlock->u.Arm.FrameBuffer = (ULONG)(unsigned long)fb_base;
    LoaderBlock->u.Arm.FrameBufferWidth = fb_width;
    LoaderBlock->u.Arm.FrameBufferHeight = fb_height;
    LoaderBlock->u.Arm.FrameBufferPitch = fb_pitch;
    LoaderBlock->u.Arm.FrameBufferPixelOrder = fb_order;

    return ESUCCESS;
}
