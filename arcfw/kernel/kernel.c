//
// NT kernel for the ARM32 / Raspberry Pi 2 port.
//
// The OS Loader (BlOsLoader) loads this PE + hal.dll, builds the LOADER_PARAMETER_BLOCK,
// and enters at KiSystemStartup with r0 = the loader block - the KE/MIPS/X4START.S
// contract. The kernel renders a boot-status screen from the data the loader handed over:
// the loaded-module list, the physical memory map, and the boot parameters. The text is
// drawn on the HDMI framebuffer through the NT HAL routine HalDisplayString (jxdisp.c,
// ported from JXDISP.C) and mirrored to the PL011 serial line (the KdPrint/debugger
// analog) for headless verification. Output goes to both via emit(). Further kernel
// initialization (the executive) is not yet implemented; it halts after the status screen.
//
// Freestanding ARMv7, no libc. Zero-initialized globals are folded into .data by
// kernel.ld so the image carries no .bss. Runs MMU-off at link address 0x01001000.
//

#include "kernel.h"

#define UART0   0x3F201000u
#define UART_DR (*(volatile ULONG *)(UART0 + 0x00))
#define UART_FR (*(volatile ULONG *)(UART0 + 0x18))
#define FR_TXFF 0x20u

static void kputc(int c)
{
    while (UART_FR & FR_TXFF)
        ;
    UART_DR = (ULONG)(unsigned char)c;
}

static void kputs(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n')
            kputc('\r');
        kputc(*s);
    }
}

//
// KiInitializeKernel's "save the address of the loader parameter block" (INITKR.C).
//
PVOID KeLoaderBlock;

// TRUE once HalpInitializeDisplay0 has a framebuffer + font; gates the HDMI sink.
static int DisplayOk;

//
// Output sink: every status line goes to the PL011 serial line AND, once the HAL
// console is up, to the HDMI framebuffer through the real HalDisplayString. Strings use
// '\n' only - kputs adds the CR for serial, HalpDisplayCharacter handles it for HDMI.
//
static void emit(const char *s)
{
    kputs(s);
    if (DisplayOk)
        HalDisplayString((PUCHAR)s);
}

static void emit_pad(const char *s, int width)
{
    int n = 0;
    emit(s);
    while (s[n])
        n += 1;
    while (n < width) {
        emit(" ");
        n += 1;
    }
}

static void emit_hex(ULONG v)
{
    static const char digits[] = "0123456789abcdef";
    char b[11];
    int i;

    b[0] = '0';
    b[1] = 'x';
    for (i = 0; i < 8; i += 1)
        b[2 + i] = digits[(v >> ((7 - i) * 4)) & 0xf];
    b[10] = '\0';
    emit(b);
}

static void emit_dec(ULONG v)
{
    char b[12];
    int i = 11;

    b[11] = '\0';
    if (v == 0) {
        emit("0");
        return;
    }
    while (v) {
        b[--i] = (char)('0' + v % 10);
        v /= 10;
    }
    emit(&b[i]);
}

// Adaptive human size from a 4 KiB page count (256 pages == 1 MiB).
static void emit_size(ULONG pages)
{
    if (pages >= 256) {
        emit_dec(pages / 256);
        emit(" MiB");
    } else {
        emit_dec(pages * 4);
        emit(" KiB");
    }
}

static void emit_str(PUCHAR s)
{
    if (s == NULL || *s == 0)
        emit("(none)");
    else
        emit((const char *)s);
}

// One module name: WCHARs (low byte = ASCII) -> a padded ASCII column.
static void emit_wname(PWSTR w, ULONG nchars, int width)
{
    char b[32];
    ULONG i;

    if (nchars > sizeof(b) - 1)
        nchars = sizeof(b) - 1;
    for (i = 0; i < nchars; i += 1)
        b[i] = (char)(w[i] & 0xff);
    b[i] = '\0';
    emit_pad(b, width);
}

// TYPE_OF_MEMORY (arc.h) value -> short label, for the memory map.
static const char *MemTypeName(ULONG t)
{
    static const char *const names[] = {
        "ExceptionBlock", "SystemBlock", "Free", "Bad", "LoadedProgram",
        "FirmwareTemp", "FirmwarePerm", "OsloaderHeap", "OsloaderStack", "SystemCode",
        "HalCode", "BootDriver", "ConsoleInDrv", "ConsoleOutDrv", "StartupDpcStk",
        "StartupKrnlStk", "StartupPanicStk", "StartupPcrPage", "StartupPdrPage",
        "RegistryData", "MemoryData", "NlsData", "SpecialMemory", "Maximum"
    };

    return (t < sizeof(names) / sizeof(names[0])) ? names[t] : "?";
}

void
KiSystemStartupC(void *LoaderBlockPtr)
{
    PLOADER_PARAMETER_BLOCK lb = (PLOADER_PARAMETER_BLOCK)LoaderBlockPtr;
    PLIST_ENTRY head, link;
    ULONG nmod = 0, ndesc = 0, totalPages = 0, freePages = 0;

    KeLoaderBlock = LoaderBlockPtr;

    //
    // Bring up the HAL display (clears the screen + reads the font/framebuffer from the
    // loader block). DisplayOk gates the HDMI half of emit(); serial always works.
    //
    DisplayOk = HalpInitializeDisplay0(lb);

    emit("Microsoft (R) Windows NT (TM)    Version 3.5  Build 782\n");
    emit("ARM32 / Raspberry Pi 2  -  ARMv7-A Cortex-A7 (BCM2836)\n");
    emit("==================================================================\n\n");

    emit("KiSystemStartup: control received from the OS Loader.\n");
    emit("  loader parameter block : ");
    emit_hex((ULONG)(unsigned long)lb);
    emit("\n  kernel stack (sp)      : ");
    emit_hex(lb->KernelStack);
    emit("\n  OEM font / framebuffer : ");
    emit_hex((ULONG)(unsigned long)lb->OemFontFile);
    emit(" / ");
    emit_hex(lb->Arm.FrameBuffer);
    emit("\n");

    //
    // Loaded modules - the images the OS Loader PE-loaded and recorded in the loader
    // block's LoadOrderList (BlAllocateDataTableEntry, ported BLBIND.C): NTOSKRNL + HAL.
    //
    emit("\nLoaded modules (OS Loader LoadOrderList):\n");
    head = &lb->LoadOrderListHead;
    for (link = head->Flink; link != head && nmod < 64; link = link->Flink) {
        PLDR_DATA_TABLE_ENTRY e = (PLDR_DATA_TABLE_ENTRY)link;  // InLoadOrderLinks @ off 0
        nmod += 1;
        emit("  ");
        emit_wname(e->BaseDllName.Buffer, e->BaseDllName.Length / 2, 14);
        emit(" base ");
        emit_hex((ULONG)(unsigned long)e->DllBase);
        emit("  size ");
        emit_hex(e->SizeOfImage);
        emit("  entry ");
        emit_hex((ULONG)(unsigned long)e->EntryPoint);
        emit("\n");
    }

    //
    // Physical memory map the loader built (BlMemoryInitialize), one descriptor per
    // region with its TYPE_OF_MEMORY. A non-empty list, intact here, is the data
    // contract proving the loader block's lists crossed the hand-off whole.
    //
    emit("\nPhysical memory (OS Loader MemoryDescriptorList):\n");
    head = &lb->MemoryDescriptorListHead;
    for (link = head->Flink; link != head && ndesc < 256; link = link->Flink) {
        PMEMORY_ALLOCATION_DESCRIPTOR d = (PMEMORY_ALLOCATION_DESCRIPTOR)link;
        ndesc += 1;
        totalPages += d->PageCount;
        if (d->MemoryType == LoaderFree)
            freePages += d->PageCount;
        emit("  ");
        emit_pad(MemTypeName(d->MemoryType), 16);
        emit(" page ");
        emit_hex(d->BasePage);
        emit("  pages ");
        emit_hex(d->PageCount);
        emit("  (");
        emit_size(d->PageCount);
        emit(")\n");
    }
    emit("  ---- ");
    emit_dec(ndesc);
    emit(" descriptors, ");
    emit_size(totalPages);
    emit(" mapped, ");
    emit_size(freePages);
    emit(" free ----\n");

    //
    // Boot parameters the loader passed through the block.
    //
    emit("\nBoot parameters:\n");
    emit("  boot device  : ");
    emit_str(lb->ArcBootDeviceName);
    emit("\n  system root  : ");
    emit_str(lb->NtBootPathName);
    emit("\n  HAL path     : ");
    emit_str(lb->NtHalPathName);
    emit("\n  load options : ");
    emit_str(lb->LoadOptions);
    emit("\n  console      : ");
    emit_dec(lb->Arm.FrameBufferWidth);
    emit(" x ");
    emit_dec(lb->Arm.FrameBufferHeight);
    emit(" x32bpp, pitch ");
    emit_dec(lb->Arm.FrameBufferPitch);
    emit("\n");

    emit("\nNTOSKRNL: OS Loader hand-off data above received intact.\n");
    emit("System halted.\n");
}
