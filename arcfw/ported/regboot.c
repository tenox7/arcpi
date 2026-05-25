/*++

ARM port: the HIVE subset of PRIVATE/NTOS/BOOT/BLDR/REGBOOT.C.

REGBOOT.C is "a minimal registry implementation designed to be used by the
osloader at boot time" - it loads the SYSTEM hive and computes the boot driver
list from it. We port only the hive functions verbatim (BlpHiveAllocate,
BlInitializeHive, BlLoadAndInitSystemHive, BlScanRegistry) plus the boot stubs
the read-only hive engine links against (HvMarkCellDirty, HvMarkDirty,
HvMarkClean, HvpDoWriteHive, HvpGrowLog1, HvpGrowLog2, CmpTestRegistryLock,
CmpTestRegistryLockExclusive, CmpValidateHiveSecurityDescriptors, HvIsBinDirty,
HvLoadHive). Those are the write, sync and lock operations that live in the
unported HIVESYNC, HIVEFREE and lock files; they are no-ops at boot.

NOT ported from REGBOOT.C (so they don't conflict / aren't needed):
  - the ANSI console helpers (BlpClearScreen/BlpPositionCursor/BlpSetInverseMode)
    - the firmware emulator already drives the console;
  - BlCheckBreakInKey - already provided by arcfw/arm/blsvc.c (Tier A);
  - BlLoadBootDrivers - boot drivers are deferred (no ARM drivers exist), and the
    Tier-B hive-scan path stops before loading them;
  - BlLastKnownGoodPrompt - the LKG UI; stubbed below (the boot path passes
    UseLastKnownGood=FALSE, so BlScanRegistry never calls it).

The CMHIVE/CmLog globals and the function bodies below are verbatim from
REGBOOT.C; see arcfw/inc/cmshim.h for the boot-CM environment shims.

--*/

#include "bldr.h"
#include "cmp.h"
#include "stdio.h"
#include "string.h"

CMHIVE BootHive;
ULONG CmLogLevel=100;
ULONG CmLogSelect=0;

//
// LastKnownGood prompt - stubbed. The osloader boot path uses Default (not LKG),
// so BlScanRegistry's only call site (UseLastKnownGood && !AutoSelect) is dead.
//
BOOLEAN
BlLastKnownGoodPrompt(
    IN OUT PBOOLEAN UseLastKnownGood
    )
{
    *UseLastKnownGood = FALSE;
    return FALSE;
}

PVOID
BlpHiveAllocate(
    IN ULONG Length,
    IN BOOLEAN UseForIo
    )

/*++

Routine Description:

    Wrapper for hive allocation calls.  It just calls BlAllocateHeap.

Arguments:

    Length - Supplies the size of block required in bytes.

    UseForIo - Supplies whether or not the memory is to be used for I/O
               (this is currently ignored)

Return Value:

    address of the block of memory
        or
    NULL if no memory available

--*/

{
    return(BlAllocateHeap(Length));

}


BOOLEAN
BlInitializeHive(
    IN PVOID HiveImage,
    IN PCMHIVE Hive,
    IN BOOLEAN IsAlternate
    )

/*++

Routine Description:

    Initializes the hive data structure based on the in-memory hive image.

Arguments:

    HiveImage - Supplies a pointer to the in-memory hive image.

    Hive - Supplies the CMHIVE structure to be filled in.

    IsAlternate - Supplies whether or not the hive is the alternate hive,
        which indicates that the primary hive is corrupt and should be
        rewritten by the system.

Return Value:

    TRUE - Hive successfully initialized.

    FALSE - Hive is corrupt.

--*/
{
    NTSTATUS    status;
    ULONG       HiveCheckCode;

    status = HvInitializeHive(
                &Hive->Hive,
                HINIT_MEMORY_INPLACE,
                FALSE,
                IsAlternate ? HFILE_TYPE_ALTERNATE : HFILE_TYPE_PRIMARY,
                HiveImage,
                (PALLOCATE_ROUTINE)BlpHiveAllocate,     // allocate
                NULL,                                   // free
                NULL,                                   // setsize
                NULL,                                   // write
                NULL,                                   // read
                NULL,                                   // flush
                1,                                      // cluster
                NULL
                );

    if (!NT_SUCCESS(status)) {
        return FALSE;
    }

    HiveCheckCode = CmCheckRegistry(Hive,FALSE);
    if (HiveCheckCode != 0) {
        return(FALSE);
    } else {
        return TRUE;
    }

}


ARC_STATUS
BlLoadAndInitSystemHive(
    IN ULONG DeviceId,
    IN PCHAR DeviceName,
    IN PCHAR DirectoryPath,
    IN PCHAR HiveName,
    IN BOOLEAN IsAlternate
    )

/*++

Routine Description:

    Loads the registry SYSTEM hive, verifies it is a valid hive file,
    and inits the relevant registry structures.  (particularly the HHIVE)

Arguments:

    DeviceId - Supplies the file id of the device the system tree is on.

    DeviceName - Supplies the name of the device the system tree is on.

    DirectoryPath - Supplies a pointer to the zero-terminated directory path
        of the root of the NT tree.

    HiveName - Supplies the name of the system hive ("SYSTEM" or "SYSTEM.ALT")

    IsAlternate - Supplies whether or not the hive to be loaded is the
        alternate hive, in which case the primary hive is corrupt and must
        be rewritten by the system.

Return Value:

    ESUCCESS is returned if the system hive was successfully loaded.
        Otherwise, an unsuccessful status is returned.

--*/

{
    ARC_STATUS Status;

    Status = BlLoadSystemHive(DeviceId,
                              DeviceName,
                              DirectoryPath,
                              HiveName);
    if (Status!=ESUCCESS) {
        return(Status);
    }

    if (!BlInitializeHive(BlLoaderBlock->RegistryBase,
                          &BootHive,
                          IsAlternate)) {
        return(EINVAL);
    }
    return(ESUCCESS);
}


PCHAR
BlScanRegistry(
    IN BOOLEAN UseLastKnownGood,
    IN PWSTR BootFileSystemPath,
    OUT PLIST_ENTRY BootDriverListHead,
    OUT PUNICODE_STRING AnsiCodepage,
    OUT PUNICODE_STRING OemCodepage,
    OUT PUNICODE_STRING LanguageTable,
    OUT PUNICODE_STRING OemHalFont
    )

/*++

Routine Description:

    Scans the SYSTEM hive and computes the list of drivers to be loaded.

Arguments:

    UseLastKnownGood - Supplies whether or not the user wishes to boot
        the last configuration known to be good.  (default is FALSE)

    BootFileSystemPath - Supplies the name of the image the filesystem
        for the boot volume was read from.  The last entry in
        BootDriverListHead will refer to this file, and to the registry
        key entry that controls it.

    BootDriverListHead - Receives a pointer to the first element of the
        list of boot drivers.  Each element in this singly linked list will
        provide the loader with two paths.  The first is the path of the
        file that contains the driver to load, the second is the path of
        the registry key that controls that driver.  Both will be passed
        to the system via the loader heap.

    AnsiCodepage - Receives the name of the ANSI codepage data file

    OemCodepage - Receives the name of the OEM codepage data file

    Language - Receives the name of the language case table data file

    OemHalfont - receives the name of the OEM font to be used by the HAL.

Return Value:

    NULL    if all is well.
    NON-NULL if the hive is corrupt or inconsistent.  Return value is a
        pointer to a string that describes what is wrong.

--*/

{
    HCELL_INDEX ControlSet;
    UNICODE_STRING ControlName;
    BOOLEAN AutoSelect;
    BOOLEAN KeepGoing;

    if (UseLastKnownGood) {
        RtlInitUnicodeString(&ControlName, L"LastKnownGood");
    } else {
        RtlInitUnicodeString(&ControlName, L"Default");
    }
    ControlSet = CmpFindControlSet(&BootHive.Hive,
                                   BootHive.Hive.BaseBlock->RootCell,
                                   &ControlName,
                                   &AutoSelect);
    if (ControlSet == HCELL_NIL) {
        return("CmpFindControlSet");
    }

    if (UseLastKnownGood && !AutoSelect) {
        KeepGoing = BlLastKnownGoodPrompt(&UseLastKnownGood);
        if (!UseLastKnownGood) {
            RtlInitUnicodeString(&ControlName, L"Default");
            ControlSet = CmpFindControlSet(&BootHive.Hive,
                                           BootHive.Hive.BaseBlock->RootCell,
                                           &ControlName,
                                           &AutoSelect);
            if (ControlSet == HCELL_NIL) {
                return("CmpFindControlSet");
            }
        }
    }

    if (!CmpFindNLSData(&BootHive.Hive,
                        ControlSet,
                        AnsiCodepage,
                        OemCodepage,
                        LanguageTable,
                        OemHalFont)) {
        return("CmpFindNLSData");
    }

    InitializeListHead(BootDriverListHead);
    if (!CmpFindDrivers(&BootHive.Hive,
                        ControlSet,
                        BootLoad,
                        BootFileSystemPath,
                        BootDriverListHead)) {
        return("CmpFindDriver");
    }

    if (!CmpSortDriverList(&BootHive.Hive,
                           ControlSet,
                           BootDriverListHead)) {
        return("Missing or invalid Control\\ServiceGroupOrder\\List registry value");
    }

    if (!CmpResolveDriverDependencies(BootDriverListHead)) {
        return("CmpResolveDriverDependencies");
    }

    return( NULL );
}


NTSTATUS
HvLoadHive(
    PHHIVE  Hive,
    PVOID   *Image
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Image);
    return(STATUS_SUCCESS);
}

BOOLEAN
HvMarkCellDirty(
    PHHIVE      Hive,
    HCELL_INDEX Cell
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Cell);
    return(TRUE);
}

BOOLEAN
HvMarkDirty(
    PHHIVE      Hive,
    HCELL_INDEX Start,
    ULONG       Length
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Start);
    UNREFERENCED_PARAMETER(Length);
    return(TRUE);
}

BOOLEAN
HvMarkClean(
    PHHIVE      Hive,
    HCELL_INDEX Start,
    ULONG       Length
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Start);
    UNREFERENCED_PARAMETER(Length);
    return(TRUE);
}

BOOLEAN
HvpDoWriteHive(
    PHHIVE          Hive,
    ULONG           FileType
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(FileType);
    return(TRUE);
}

BOOLEAN
HvpGrowLog1(
    PHHIVE  Hive,
    ULONG   Count
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Count);
    return(TRUE);
}

BOOLEAN
HvpGrowLog2(
    PHHIVE  Hive,
    ULONG   Size
    )
{
    UNREFERENCED_PARAMETER(Hive);
    UNREFERENCED_PARAMETER(Size);
    return(TRUE);
}

BOOLEAN
CmpValidateHiveSecurityDescriptors(
    IN PHHIVE Hive
    )
{
    UNREFERENCED_PARAMETER(Hive);
    return(TRUE);
}


BOOLEAN
CmpTestRegistryLock()
{
    return TRUE;
}

BOOLEAN
CmpTestRegistryLockExclusive()
{
    return TRUE;
}


BOOLEAN
HvIsBinDirty(
IN PHHIVE Hive,
IN HCELL_INDEX Cell
)
{
    return(FALSE);
}
