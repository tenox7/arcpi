//
// ARM ARC firmware-vector emulation.
//
// The Raspberry Pi, like a PC, has no ARC ROM firmware. As NT does on x86
// (BOOT/LIB/I386/ARCEMUL.C), the loader emulates the firmware vector in its own
// memory: SYSTEM_BLOCK resolves to &GlobalSystemBlock (see the _ARM_ branch in
// arc.h), and the Arc* call macros dispatch through GlobalFirmwareVectors[].
//
// Every slot starts pointing at BlArcNotYetImplemented; the console/memory/halt
// subset is replaced with real AE* routines backed by RPi2 hardware.
//
#include "bldr.h"
#include "string.h"

//
// PL011 primitives (loader/arm/uart.c).
//
// Two distinct NT lineages meet in the AE* routines below, and it is worth
// keeping them straight:
//
//   * The vector-emulation scaffolding - building GlobalSystemBlock in RAM,
//     dispatching through GlobalFirmwareVectors[], and mapping the ArcOpen
//     contract onto fixed file ids - is the x86 pattern (I386/ARCEMUL.C). x86 is
//     the *only* NT arch whose loader emulates the firmware vector, because it
//     is the only one with no ARC ROM; MIPS/Alpha/PPC loaders consume a vector
//     the ROM already placed at a fixed address (see SYSTEM_BLOCK in arc.h). The
//     Pi has no ROM either, so this is the lineage we inherit for the plumbing.
//
//   * The device backend - actually moving bytes - is NOT the x86 BIOS (int
//     10h/16h, meaningless here). It is a memory-mapped serial driver, i.e. the
//     ARM analog of the RISC ARC firmware's serial line driver FW/MIPS/JXSERIAL.C
//     (SerialOpen/Read/Write over UART registers). Our backend is the PL011.
//
void uart_putc(int c);
int uart_getc(void);
int uart_rx_ready(void);
void console_putc(int c);

//
// USB keyboard input (loader/arm/usb.c). The console is bidirectional like the RISC
// firmware's serial line, but its *input* side now has two backends: the PL011 RX
// (serial cable) and a USB HID keyboard. AERead/AEReadStatus poll both so the loader
// takes keystrokes from whichever is present - a serial host in QEMU, or a USB
// keyboard plugged into a standalone Pi. usb_kbd_rx_ready() drives one HID poll and
// reports whether a decoded byte is buffered; usb_kbd_getc() pops it.
//
int usb_kbd_present(void);
int usb_kbd_rx_ready(void);
int usb_kbd_getc(void);

//
// Framebuffer console cursor query (loader/arm/fbcon.c), 0-based. Backs
// AEGetDisplayStatus, the ArcGetDisplayStatus vector slot the arcdos line editor
// uses to anchor its prompt and redraw the input line.
//
void fbcon_status(unsigned int *col, unsigned int *row,
                  unsigned int *maxcol, unsigned int *maxrow);

//
// The console is a headless serial line. The RISC ARC firmware names its serial
// ports "multi(0)serial(N)" (FW/MIPS/JXSERIAL.C); we expose port 0 as the one
// bidirectional console device, and init.c points both consolein= and
// consoleout= at it. A serial port carries both directions, so unlike the x86
// split (video out + keyboard in) there is a single name; AEOpen maps the open
// mode to the input/output file id the rest of the emulation uses.
//
#define SERIAL_CONSOLE_NAME "multi(0)serial(0)"

#define ARC_CONSOLE_INPUT  0
#define ARC_CONSOLE_OUTPUT 1

//
// The firmware vector and the system parameter block that points at it.
// Mirrors the static initialization in I386/ARCEMUL.C.
//
// GlobalFirmwareVectors lives in BSS, so every slot is NULL until
// BlArmInitializeArcEmulator() runs. Do NOT issue any Arc* call (which
// dispatches through this vector) before that init runs - BlStartup() calls it
// first thing, before BlOsLoader().
//
PVOID GlobalFirmwareVectors[MaximumRoutine];

SYSTEM_PARAMETER_BLOCK GlobalSystemBlock =
    {
        0,                              // Signature
        sizeof(SYSTEM_PARAMETER_BLOCK), // Length
        0,                              // Version
        0,                              // Revision
        NULL,                           // RestartBlock
        NULL,                           // DebugBlock
        NULL,                           // GenerateExceptionVector
        NULL,                           // TlbMissExceptionVector
        MaximumRoutine,                 // FirmwareVectorLength
        GlobalFirmwareVectors,          // Pointer to vector block
        0,                              // VendorVectorLength
        NULL                            // Pointer to vendor vector block
    };

//
// BlLoaderBlock now lives in the ported BLMEMORY.C, which allocates it from the
// loader heap in BlMemoryInitialize() and owns it. The BSS placeholder that used
// to sit here is gone (it would now be a duplicate symbol).
//

ARC_STATUS
BlArcNotYetImplemented(
    IN ULONG FileId
    )
{
    BlPrint("ERROR - Unimplemented Firmware Vector called (FID %lx)\n", FileId);
    return EINVAL;
}

//
// AEOpen - emulate ArcOpen. The dispatch/contract shape follows I386/ARCEMUL.C
// (recognize the device, hand back a fixed file id); the device itself is the
// serial console (FW/MIPS/JXSERIAL.C SerialOpen). One bidirectional serial port
// serves both console directions, so open mode - not the path - selects the
// input vs output file id. Disk/partition opens will chain in here later; until
// then anything else is ENOENT.
//
ARC_STATUS
AEOpen(
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    OUT PULONG FileId
    )
{
    if (stricmp(OpenPath, SERIAL_CONSOLE_NAME) != 0) {
        return ENOENT;
    }

    if (OpenMode == ArcOpenReadOnly) {
        *FileId = ARC_CONSOLE_INPUT;
        return ESUCCESS;
    }

    if (OpenMode == ArcOpenWriteOnly) {
        *FileId = ARC_CONSOLE_OUTPUT;
        return ESUCCESS;
    }

    return EACCES;
}

//
// AEClose (JXSERIAL.C SerialClose analog) - the serial console holds no state to
// tear down, so closing either direction just succeeds. Non-console ids are not
// openable yet, so reaching here with one is a bug.
//
ARC_STATUS
AEClose(
    IN ULONG FileId
    )
{
    if (FileId == ARC_CONSOLE_INPUT || FileId == ARC_CONSOLE_OUTPUT) {
        return ESUCCESS;
    }

    return BlArcNotYetImplemented(FileId);
}

//
// AEWrite (JXSERIAL.C SerialWrite analog) - emulate ArcWrite. Only console
// output is implemented: push the exact byte count to the PL011 TX (the loader
// supplies its own CR/LF), report it all written. This is the routine
// BlOsLoader's "OS Loader V3.5" banner reaches.
//
ARC_STATUS
AEWrite(
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )
{
    const unsigned char *p;
    ULONG i;

    if (FileId != ARC_CONSOLE_OUTPUT) {
        return BlArcNotYetImplemented(FileId);
    }

    p = (const unsigned char *)Buffer;
    for (i = 0; i < Length; i += 1) {
        console_putc(p[i]);
    }

    *Count = Length;
    return ESUCCESS;
}

//
// AERead (JXSERIAL.C SerialRead analog) - emulate ArcRead. Only console input is
// implemented: a blocking read of Length raw bytes, taken from whichever console
// input backend has a byte first - the PL011 RX or the USB keyboard. Gated on the
// console id (the ARCEMUL emulation convention); any other id is a not-yet-ported
// device, never a hang.
//
ARC_STATUS
AERead(
    IN ULONG FileId,
    OUT PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )
{
    unsigned char *p;
    ULONG i;

    if (FileId != ARC_CONSOLE_INPUT) {
        return BlArcNotYetImplemented(FileId);
    }

    p = (unsigned char *)Buffer;
    for (i = 0; i < Length; i += 1) {
        for (;;) {
            if (uart_rx_ready()) {
                p[i] = (unsigned char)uart_getc();
                break;
            }
            if (usb_kbd_rx_ready()) {
                int c = usb_kbd_getc();
                if (c >= 0) {
                    p[i] = (unsigned char)c;
                    break;
                }
            }
        }
    }

    *Count = Length;
    return ESUCCESS;
}

//
// AEReadStatus (JXSERIAL.C SerialGetReadStatus analog) - emulate ArcGetReadStatus.
// ESUCCESS means at least one byte is available on the console - either the PL011 RX
// is not empty or the USB keyboard has a decoded byte buffered; EAGAIN means none yet.
//
ARC_STATUS
AEReadStatus(
    IN ULONG FileId
    )
{
    if (FileId != ARC_CONSOLE_INPUT) {
        return BlArcNotYetImplemented(FileId);
    }

    if (uart_rx_ready() || usb_kbd_rx_ready()) {
        return ESUCCESS;
    }

    return EAGAIN;
}

//
// AEGetMemoryDescriptor - emulate ArcGetMemoryDescriptor by walking the static
// MDArray[] built in loader/arm/memory.c (verbatim shape of I386/ARCEMUL.C):
// NULL returns the first descriptor, otherwise the next, NULL past the end.
//
extern MEMORY_DESCRIPTOR MDArray[];
extern ULONG NumberDescriptors;

PMEMORY_DESCRIPTOR
AEGetMemoryDescriptor(
    IN PMEMORY_DESCRIPTOR MemoryDescriptor OPTIONAL
    )
{
    if (MemoryDescriptor == NULL) {
        return MDArray;
    }

    if ((ULONG)(MemoryDescriptor - MDArray) >= (NumberDescriptors - 1)) {
        return NULL;
    }

    return ++MemoryDescriptor;
}

//
// AEReboot - emulate ArcReboot/ArcRestart via the BCM2836 PM watchdog
// (0x3F100000): arm a short watchdog, then request a full reset. All writes
// carry the 0x5A password in the high byte. Does not return. Wired for the halt
// path; the current load flow never calls it, so it is untested at runtime.
//
#define PM_BASE     0x3F100000u
#define PM_RSTC     (*(volatile unsigned int *)(PM_BASE + 0x1c))
#define PM_WDOG     (*(volatile unsigned int *)(PM_BASE + 0x24))
#define PM_PASSWORD 0x5a000000u
#define PM_RSTC_WRCFG_CLR        0xffffffcfu
#define PM_RSTC_WRCFG_FULL_RESET 0x00000020u

VOID
AEReboot(
    VOID
    )
{
    PM_WDOG = PM_PASSWORD | 10;
    PM_RSTC = PM_PASSWORD | (PM_RSTC & PM_RSTC_WRCFG_CLR) | PM_RSTC_WRCFG_FULL_RESET;

    for (;;) {
    }
}

//
// AEGetDisplayStatus - emulate ArcGetDisplayStatus (the JXDISP.C FwGetDisplayStatus
// analog). Reports the framebuffer console cursor and extent; the arcdos editor reads
// it to anchor each redraw. fbcon tracks the cursor 0-based, so add 1 to match the ARC
// 1-based convention - exactly what FwGetDisplayStatus does. The fixed white-on-blue
// scheme is reported as high-intensity white on blue. FileId is ignored (status is a
// property of the one screen), as on the RISC firmware.
//
static ARC_DISPLAY_STATUS DisplayStatus;

PARC_DISPLAY_STATUS
AEGetDisplayStatus(
    IN ULONG FileId
    )
{
    unsigned int col, row, maxcol, maxrow;

    (void)FileId;
    fbcon_status(&col, &row, &maxcol, &maxrow);

    DisplayStatus.CursorXPosition = (USHORT)(col + 1);
    DisplayStatus.CursorYPosition = (USHORT)(row + 1);
    DisplayStatus.CursorMaxXPosition = (USHORT)(maxcol + 1);
    DisplayStatus.CursorMaxYPosition = (USHORT)(maxrow + 1);
    DisplayStatus.ForegroundColor = 7;          // white
    DisplayStatus.BackgroundColor = 4;          // blue
    DisplayStatus.HighIntensity = 1;
    DisplayStatus.Underscored = 0;
    DisplayStatus.ReverseVideo = 0;

    return &DisplayStatus;
}

//
// AEGetEnvironmentVariable - emulate ArcGetEnvironmentVariable. No ARC environment
// is implemented yet, so every lookup misses (returns NULL). This MUST be a real slot
// (not BlArcNotYetImplemented): arcdos's ParseCommandLine calls it for any token with a
// ':' (e.g. "cd c:") before ever reaching Open, and the stub's ARC_STATUS(ULONG)
// signature is wrong for a PCHAR(PCHAR) call - it would hand back EINVAL(=7) as a char*
// and crash strlen(). NULL is the right miss: arcdos maps it to "Undefined environment
// variable" and re-prompts cleanly.
//
PCHAR
AEGetEnvironmentVariable(
    IN PCHAR Variable
    )
{
    (void)Variable;
    return NULL;
}

//
// Populate the firmware vector. ARM analog of I386/ARCEMUL.C's vector fill.
// Every slot starts at BlArcNotYetImplemented; the implemented routines below
// override the console (open/close/read/read-status/write), memory, and halt slots.
//
VOID
BlArmInitializeArcEmulator(
    VOID
    )
{
    ULONG cnt;

    for (cnt = 0; cnt < MaximumRoutine; cnt += 1) {
        GlobalFirmwareVectors[cnt] = (PVOID)BlArcNotYetImplemented;
    }

    GlobalFirmwareVectors[OpenRoutine]       = (PVOID)AEOpen;
    GlobalFirmwareVectors[CloseRoutine]      = (PVOID)AEClose;
    GlobalFirmwareVectors[ReadRoutine]       = (PVOID)AERead;
    GlobalFirmwareVectors[ReadStatusRoutine] = (PVOID)AEReadStatus;
    GlobalFirmwareVectors[WriteRoutine]      = (PVOID)AEWrite;
    GlobalFirmwareVectors[MemoryRoutine]     = (PVOID)AEGetMemoryDescriptor;
    GlobalFirmwareVectors[RestartRoutine]    = (PVOID)AEReboot;
    GlobalFirmwareVectors[RebootRoutine]     = (PVOID)AEReboot;
    GlobalFirmwareVectors[GetDisplayStatusRoutine] = (PVOID)AEGetDisplayStatus;
    GlobalFirmwareVectors[GetEnvironmentRoutine]   = (PVOID)AEGetEnvironmentVariable;
}
