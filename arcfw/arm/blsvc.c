//
// arcfw/arm/blsvc.c - loader services the ARM firmware emulator implements itself,
// because there is no usable portable NT source to port for them:
//
//  - BlCheckBreakInKey is machine-specific in NT (no portable definition exists);
//    it polls the console for a pending key and reports whether a boot break-in was
//    requested. We do not implement the debugger / LastKnownGood menu, so it consumes
//    any pending key and always returns FALSE (continue the normal boot).
//
//  - BlInitResources / BlFindMessage are NT's BLRES.C, which reads a MESSAGE_RESOURCE
//    catalog from the loader image's PE .rsrc section (compiled by mc.exe from msg.mc).
//    We are a flat binary with neither a .rsrc section nor a message compiler, so
//    BlInitResources is a no-op success and BlFindMessage maps the msg.h IDs to text
//    directly. This makes BlFatalError report a readable cause instead of a bare code.
//    (Returning ESUCCESS from BlInitResources also stops osloader.c:316 from raising a
//    spurious BlFatalError on every boot, which the old ENODEV stub did.)
//
#include "bldr.h"
#include "msg.h"

extern ULONG BlConsoleInDeviceId;

BOOLEAN
BlCheckBreakInKey(
    VOID
    )
{
    UCHAR Key;
    ULONG Count;

    if (ArcGetReadStatus(BlConsoleInDeviceId) == ESUCCESS) {
        ArcRead(BlConsoleInDeviceId, &Key, 1, &Count);
    }
    return FALSE;
}

ARC_STATUS
BlInitResources(
    PCHAR StartCommand
    )
{
    return ESUCCESS;
}

PCHAR
BlFindMessage(
    ULONG Id
    )
{
    switch (Id) {

    case LOAD_HW_MEM_CLASS:
        return "Windows NT could not read the system memory configuration.";
    case LOAD_HW_MEM_ACT:
        return "Check the amount of memory installed in the system.";
    case LOAD_HW_DISK_CLASS:
        return "Windows NT could not read from the selected boot disk.";
    case LOAD_HW_DISK_ACT:
        return "Check the boot path and disk hardware.";
    case LOAD_HW_FW_CFG_CLASS:
        return "Windows NT could not read the firmware configuration.";
    case LOAD_HW_FW_CFG_ACT:
        return "Check the ARC firmware (emulator) configuration.";
    case LOAD_SW_MIS_FILE_CLASS:
        return "Windows NT could not start because a required file is missing.";
    case LOAD_SW_BAD_FILE_CLASS:
        return "Windows NT could not start because a required file is corrupt.";
    case LOAD_SW_INT_ERR_CLASS:
        return "Windows NT could not start due to an internal loader error.";
    case LOAD_SW_INT_ERR_ACT:
        return "This indicates a loader bug; please report it.";
    case LOAD_SW_FILE_REINST_ACT:
        return "Reinstall the indicated file.";
    case LOAD_SW_FILE_REST_ACT:
        return "Restore the indicated file from the installation media.";

    case DIAG_BL_MEMORY_INIT:
        return "initializing memory";
    case DIAG_BL_IO_INIT:
        return "initializing the I/O system";
    case DIAG_BL_CONFIG_INIT:
        return "initializing the configuration tree";
    case DIAG_BL_FW_GET_BOOT_DEVICE:
        return "resolving the boot device name";
    case DIAG_BL_FW_GET_SYSTEM_DEVICE:
        return "resolving the system device name";
    case DIAG_BL_FW_OPEN_SYSTEM_DEVICE:
        return "opening the system device";
    case DIAG_BL_OPEN_BOOT_DEVICE:
        return "opening the boot device";
    case DIAG_BL_FIND_HAL_IMAGE:
        return "locating the HAL image";
    case DIAG_BL_LOAD_SYSTEM_IMAGE:
        return "loading the kernel image (ntoskrnl.exe)";
    case DIAG_BL_LOAD_HAL_IMAGE:
        return "loading the HAL image (hal.dll)";
    case DIAG_BL_LOAD_SYSTEM_DLLS:
        return "loading kernel-mode DLLs";
    case DIAG_BL_LOAD_HAL_DLLS:
        return "loading HAL DLLs";
    case DIAG_BL_LOAD_SYSTEM_HIVE:
        return "loading the SYSTEM hive";
    case DIAG_BL_LOAD_SYSTEM_REGISTRY_HIVE:
        return "loading the system registry hive";
    case DIAG_BL_ARC_BOOT_DEV_NAME:
        return "generating the ARC boot device name";
    case DIAG_BL_SETUP_FOR_NT:
        return "running architecture-specific setup (BlSetupForNt)";
    case DIAG_BL_KERNEL_INIT_XFER:
        return "transferring control to the kernel";

    default:
        return NULL;
    }
}
