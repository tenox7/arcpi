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

// BlConfigurationInitialize is no longer stubbed - ported real in
// arcfw/ported/blconfig.c. It walks the firmware config tree (synthesized by
// arcfw/arm/config.c and served via the GetChild/GetPeer/GetParent/GetData vector
// slots) and copies it into BlLoaderBlock->ConfigurationRoot.

// BlInitResources + BlFindMessage are no longer stubbed - implemented in
// arcfw/arm/blsvc.c (BlInitResources returns ESUCCESS; BlFindMessage maps the msg.h
// IDs to text so BlFatalError reports a readable cause). No PE .rsrc / mc.exe here.

// KeFindConfigurationEntry is no longer stubbed - ported real (with
// KeFindConfigurationNextEntry) from KE/CONFIG.C in arcfw/ported/keconfig.c; it
// searches the now-real ConfigurationRoot tree and is NULL-root-safe.

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
// BLBIND.C), so the BlOsLoader path builds the loaded-module list and resolves imports.
//

// RtlImageNtHeader is no longer stubbed - real in arcfw/arm/imageldr.c.

// ---- registry / NLS / drivers ----

// BlLoadAndInitSystemHive + BlScanRegistry are no longer stubbed - ported real (the
// hive subset of REGBOOT.C) in arcfw/ported/regboot.c, backed by NT's boot CM engine
// (the 13 bconfig.lib files in arcfw/ported/config/). Tier B: load + validate + scan
// the real SYSTEM hive into a boot driver list.

// BlLoadNLSData + BlLoadOemHalFont are no longer stubbed - ported real (verbatim
// BLLOAD.C) in arcfw/ported/blload.c. They run only when the hive-scan path is taken
// to completion (Tier C); the Tier-B path stops after the registry scan.

ARC_STATUS BlLoadBootDrivers(ULONG DeviceId, PCHAR LoadDevice, PCHAR SystemPath,
                             PLIST_ENTRY BootDriverListHead, PCHAR BadFileName)
{
    STUB("BlLoadBootDrivers");
    return ENODEV;
}

// ---- device names / disk info / break-in / setup ----

// BlGenerateDeviceNames is no longer stubbed - ported real (the verbatim ARC-name
// parsing slice of BLCONFIG.C) in arcfw/ported/blconfig.c. It validates + canonicalizes
// the ARC device name and emits the NT prefix (\Device\Harddisk etc.); osloader.c stores
// the canonical name as ArcBootDeviceName/ArcHalDeviceName.

// BlGetArcDiskInformation is no longer stubbed - ported real (verbatim ARCDISK.C)
// in arcfw/ported/arcdisk.c; it finds the ramdisk via the config tree and records
// its MBR signature in BlLoaderBlock->ArcDiskInformation.

// BlCheckBreakInKey is no longer stubbed - implemented in arcfw/arm/blsvc.c (polls the
// console for a pending key, consumes it, and returns FALSE = no boot break-in).

// BlLastKnownGoodPrompt is no longer stubbed - provided (as a no-LKG stub) in
// arcfw/ported/regboot.c, where BlScanRegistry's lone call site lives.

// BlSetupForNt is no longer stubbed - real (minimal) in arcfw/arm/ntsetup.c (the
// ARM analog of BOOT/LIB/MIPS/NTSETUP.C): it allocates the kernel stack the kernel
// entry switches to.
