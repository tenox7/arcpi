//
// Stubs for the BOOT/LIB and kernel routines BlOsLoader() calls that are not
// yet ported - just enough to satisfy the link so the loader builds as one image.
// Each announces itself over the console and returns a failure/empty result, and
// is replaced by a real implementation as the console, memory, storage, and
// image-load paths come up.
//
// Prototypes come from bldr.h (Bl*) and portdecls.h (Rtl*/Ke*), so these
// definitions must match those signatures exactly.
//
#include "bldr.h"

#define STUB(name) BlPrint("  [stub] " name "\n")

// ---- argument / configuration / resources ----
//
// BlGetArgumentValue is no longer here - ported real in arcfw/ported/blmisc.c
// (the first BOOT/LIB port). OSLOADER calls it before opening the console, so
// it had to become real for BlOsLoader to get past its first line.
//

// Stubbed to ESUCCESS, not ported: BlConfigurationInitialize builds the ARC
// configuration tree (BLCONFIG.C, 904 lines). osloader.c sets
// BlLoaderBlock->ConfigurationRoot = NULL right before calling it and never
// reads ConfigurationRoot again on the path to loading the kernel (the only
// readers are the KeFindConfigurationEntry cache lookups, also stubbed to NULL,
// which fall through to defaults). So a no-op success is safe for now.
ARC_STATUS BlConfigurationInitialize(PCONFIGURATION_COMPONENT Parent,
                                     PCONFIGURATION_COMPONENT_DATA ParentEntry)
{
    return ESUCCESS;
}

ARC_STATUS BlInitResources(PCHAR StartCommand)
{
    STUB("BlInitResources");
    return ENODEV;
}

PCHAR BlFindMessage(ULONG Id)
{
    STUB("BlFindMessage");
    return NULL;
}

PCONFIGURATION_COMPONENT_DATA KeFindConfigurationEntry(PCONFIGURATION_COMPONENT_DATA Child,
                                                       CONFIGURATION_CLASS Class,
                                                       CONFIGURATION_TYPE Type,
                                                       PULONG Key)
{
    STUB("KeFindConfigurationEntry");
    return NULL;
}

// ---- memory / heap ----
//
// BlMemoryInitialize and BlAllocateHeap are no longer stubbed - both are ported
// real in arcfw/ported/blmemory.c, backed by AEGetMemoryDescriptor over the ARM
// memory map in arcfw/arm/memory.c.
//

// ---- I/O / filesystem ----
//
// BlIoInitialize and BlGetFsInfo are no longer stubbed - both are ported real in
// arcfw/ported/blio.c. BlIoInitialize inits the BL_FILE_TABLE and calls the FS
// initializers (FAT will be real once FATBOOT.C is ported; others stubbed silent
// in fsstub.c).
//

// ---- image loading / binding ----
//
// BlLoadImage is no longer stubbed - ported real in arcfw/ported/peldr.c (the NT
// PE loader). RtlImageNtHeader (also formerly stubbed here) and
// RtlImageDirectoryEntryToData + the LdrRelocateImage guard are in
// arcfw/arm/imageldr.c. BlAllocateDataTableEntry / BlScanImportDescriptorTable are
// no longer stubbed either - ported real in arcfw/ported/blbind.c (verbatim NT
// BLBIND.C), so the genuine BlOsLoader path ([3]) builds the loaded-module list and
// resolves imports. (The trimmed kernel handoff [1] still doesn't need them.)
//

// RtlImageNtHeader is no longer stubbed - real in arcfw/arm/imageldr.c.

// ---- registry / NLS / drivers ----

ARC_STATUS BlLoadAndInitSystemHive(ULONG DeviceId, PCHAR DeviceName, PCHAR DirectoryPath,
                                   PCHAR HiveName, BOOLEAN IsAlternate)
{
    STUB("BlLoadAndInitSystemHive");
    return ENODEV;
}

PCHAR BlScanRegistry(BOOLEAN UseLastKnownGood, PWSTR BootFileSystemPath,
                     PLIST_ENTRY BootDriverListHead, PUNICODE_STRING AnsiCodepage,
                     PUNICODE_STRING OemCodepage, PUNICODE_STRING LanguageTable,
                     PUNICODE_STRING OemHalFont)
{
    STUB("BlScanRegistry");
    return "boot-loader registry scan not implemented";
}

ARC_STATUS BlLoadNLSData(ULONG DeviceId, PCHAR DeviceName, PCHAR DirectoryPath,
                         PUNICODE_STRING AnsiCodepage, PUNICODE_STRING OemCodepage,
                         PUNICODE_STRING LanguageTable, PCHAR BadFileName)
{
    STUB("BlLoadNLSData");
    return ENODEV;
}

ARC_STATUS BlLoadOemHalFont(ULONG DeviceId, PCHAR DeviceName, PCHAR DirectoryPath,
                            PUNICODE_STRING OemHalFont, PCHAR BadFileName)
{
    STUB("BlLoadOemHalFont");
    return ENODEV;
}

ARC_STATUS BlLoadBootDrivers(ULONG DeviceId, PCHAR LoadDevice, PCHAR SystemPath,
                             PLIST_ENTRY BootDriverListHead, PCHAR BadFileName)
{
    STUB("BlLoadBootDrivers");
    return ENODEV;
}

// ---- device names / disk info / break-in / setup ----

// Stand-in: the real BlGenerateDeviceNames builds the ARC<->NT device-name mapping that
// osloader.c stores in ArcBootDeviceName/ArcHalDeviceName/NtBootPathName. We have no NT
// device tree, so echo the ARC device name into the canonical output (gives the kernel's
// boot screen a real device name) and leave the NT prefix empty; return ESUCCESS (an
// error would fail the load just before BlSetupForNt). osloader.c strlen/strcpy's both.
ARC_STATUS BlGenerateDeviceNames(PCHAR ArcDeviceName, PCHAR ArcCanonicalName,
                                 PCHAR NtDevicePrefix)
{
    if (ArcCanonicalName) {
        ULONG i = 0;
        if (ArcDeviceName)
            for (; ArcDeviceName[i]; i += 1)
                ArcCanonicalName[i] = ArcDeviceName[i];
        ArcCanonicalName[i] = '\0';
    }
    if (NtDevicePrefix)
        NtDevicePrefix[0] = '\0';
    return ESUCCESS;
}

ARC_STATUS BlGetArcDiskInformation(VOID)
{
    STUB("BlGetArcDiskInformation");
    return ENODEV;
}

BOOLEAN BlCheckBreakInKey(VOID)
{
    STUB("BlCheckBreakInKey");
    return FALSE;
}

BOOLEAN BlLastKnownGoodPrompt(PBOOLEAN UseLastKnownGood)
{
    STUB("BlLastKnownGoodPrompt");
    return TRUE;
}

// BlSetupForNt is no longer stubbed - real (minimal) in arcfw/arm/ntsetup.c (the
// ARM analog of BOOT/LIB/MIPS/NTSETUP.C): it allocates the kernel stack the kernel
// entry switches to.
