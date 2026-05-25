#ifndef _ARM_CMSHIM_
#define _ARM_CMSHIM_

//
// Environment shim for the boot-time Configuration Manager subset (bconfig.lib:
// the 13 CONFIG files ported into arcfw/ported/config/, plus regboot.c). The real
// kernel build gets these from the full ke/ex/rtl header set; our minimal ntos.h
// does not, so we supply exactly what the read-only boot hive engine references.
//
// Included from our cmp.h copy, after ntos.h (which provides ULONG/NTSTATUS/
// UNICODE_STRING/LIST_ENTRY). Scope was measured against the 13 files:
//   - debug macros: ASSERT/KdPrint -> no-ops (CMLOG is gated inside cmp.h itself).
//   - POOL_TYPE: the allocate-routine argument type (only ever passed, never used).
//   - 10 STATUS_* codes (values verbatim from PUBLIC/SDK/INC/NTSTATUS.H).
//   - RTL_BITMAP + the 4 bitmap routines HvInitializeHive uses.
//   - the 7 UNICODE_STRING/ANSI string routines the engine calls.
// The string + bitmap routines are implemented in arcfw/arm/ntrtlcm.c.
//

//
// Debug macros (this is a non-DBG build). KdPrint/ASSERT vanish; the 4 DbgPrint
// references in the engine are all commented out, so DbgPrint is not declared.
//
#ifndef ASSERT
#define ASSERT(exp)
#endif
#ifndef KdPrint
#define KdPrint(_x_)
#endif

//
// POOL_TYPE (PUBLIC/SDK/INC/NTDEF.H). The boot CM passes it to the hive allocate
// routine, which ignores it (BlpHiveAllocate -> BlAllocateHeap).
//
typedef enum _POOL_TYPE {
    NonPagedPool,
    PagedPool,
    NonPagedPoolMustSucceed,
    DontUseThisType,
    NonPagedPoolCacheAligned,
    PagedPoolCacheAligned,
    NonPagedPoolCacheAlignedMustS
} POOL_TYPE;

//
// NTSTATUS codes the 13 files reference (values from PUBLIC/SDK/INC/NTSTATUS.H).
//
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#endif
#define STATUS_REPARSE                   ((NTSTATUS)0x00000104L)
#define STATUS_REGISTRY_RECOVERED        ((NTSTATUS)0x40000009L)
#define STATUS_NO_MORE_ENTRIES           ((NTSTATUS)0x8000001AL)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#define STATUS_NO_MEMORY                 ((NTSTATUS)0xC0000017L)
#define STATUS_OBJECT_NAME_NOT_FOUND     ((NTSTATUS)0xC0000034L)
#define STATUS_INSUFFICIENT_RESOURCES    ((NTSTATUS)0xC000009AL)
#define STATUS_REGISTRY_CORRUPT          ((NTSTATUS)0xC000014CL)
#define STATUS_REGISTRY_IO_FAILED        ((NTSTATUS)0xC000014DL)

//
// RTL bitmap (NTRTL.H). HvInitializeHive builds the hive's dirty/free vector with
// these; implemented in ntrtlcm.c.
//
typedef struct _RTL_BITMAP {
    ULONG SizeOfBitMap;                 // number of bits
    PULONG Buffer;                      // the bit array
} RTL_BITMAP, *PRTL_BITMAP;

VOID RtlInitializeBitMap(IN PRTL_BITMAP BitMapHeader, IN PULONG BitMapBuffer, IN ULONG SizeOfBitMap);
VOID RtlClearAllBits(IN PRTL_BITMAP BitMapHeader);
VOID RtlSetAllBits(IN PRTL_BITMAP BitMapHeader);
ULONG RtlNumberOfSetBits(IN PRTL_BITMAP BitMapHeader);

//
// UNICODE_STRING / ANSI_STRING runtime routines the engine calls (NTRTL.H).
// Implemented in ntrtlcm.c. RtlInitString is already declared in portdecls.h.
//
VOID RtlInitUnicodeString(OUT PUNICODE_STRING DestinationString, IN PCWSTR SourceString);
NTSTATUS RtlAppendUnicodeToString(IN OUT PUNICODE_STRING Destination, IN PCWSTR Source);
NTSTATUS RtlAppendUnicodeStringToString(IN OUT PUNICODE_STRING Destination, IN PUNICODE_STRING Source);
BOOLEAN RtlEqualUnicodeString(IN PUNICODE_STRING String1, IN PUNICODE_STRING String2, IN BOOLEAN CaseInSensitive);
LONG RtlCompareUnicodeString(IN PUNICODE_STRING String1, IN PUNICODE_STRING String2, IN BOOLEAN CaseInSensitive);
NTSTATUS RtlAnsiStringToUnicodeString(IN OUT PUNICODE_STRING DestinationString, IN PANSI_STRING SourceString, IN BOOLEAN AllocateDestinationString);
WCHAR RtlUpcaseUnicodeChar(IN WCHAR SourceCharacter);

//
// Opaque forward types. cmp.h declares them as members of runtime-registry
// structures (CM_KEY_BODY/CM_POST_BLOCK/CM_PARSE_CONTEXT) that the boot subset
// never instantiates; they only need to parse. (The full ntos.h would bring the
// real definitions via ob.h/ke.h/se.h.)
//
typedef PVOID POBJECT_TYPE;
typedef PVOID PKEVENT;
typedef PVOID PKAPC;
typedef PVOID PETHREAD;
typedef PVOID PEPROCESS;
typedef PVOID POBJECT_NAME_INFORMATION;
typedef PVOID PSECURITY_CLIENT_CONTEXT;
typedef PVOID PACCESS_STATE;
typedef PVOID PSECURITY_QUALITY_OF_SERVICE;
typedef CCHAR KPROCESSOR_MODE;
typedef struct _SECURITY_SUBJECT_CONTEXT { PVOID Reserved[4]; } SECURITY_SUBJECT_CONTEXT;

//
// SECURITY_DESCRIPTOR (PUBLIC/SDK/INC/WINNT.H, absolute form). CM_KEY_SECURITY
// (the hive security cell) has one as a direct member, so it needs a complete
// type. The boot read path navigates keys/values, not security, so only its
// SIZE matters here; this is the standard 20-byte 32-bit layout.
//
typedef USHORT SECURITY_DESCRIPTOR_CONTROL, *PSECURITY_DESCRIPTOR_CONTROL;
typedef struct _SECURITY_DESCRIPTOR {
    UCHAR Revision;
    UCHAR Sbz1;
    SECURITY_DESCRIPTOR_CONTROL Control;
    PVOID Owner;
    PVOID Group;
    PVOID Sacl;
    PVOID Dacl;
} SECURITY_DESCRIPTOR, *PSECURITY_DESCRIPTOR;

//
// Service node/load/error enumerations + the Win32 SERVICE_* constants they map
// to (PUBLIC/SDK/INC/{WINNT.H,NTCONFIG.H}). CmpFindDrivers takes a
// SERVICE_LOAD_TYPE; CmpFindDrivers/CmpIsLoadType test the Type/Start values.
//
#define SERVICE_KERNEL_DRIVER          0x00000001
#define SERVICE_FILE_SYSTEM_DRIVER     0x00000002
#define SERVICE_ADAPTER                0x00000004
#define SERVICE_RECOGNIZER_DRIVER      0x00000008
#define SERVICE_WIN32_OWN_PROCESS      0x00000010
#define SERVICE_WIN32_SHARE_PROCESS    0x00000020

#define SERVICE_BOOT_START             0x00000000
#define SERVICE_SYSTEM_START           0x00000001
#define SERVICE_AUTO_START             0x00000002
#define SERVICE_DEMAND_START           0x00000003
#define SERVICE_DISABLED               0x00000004

#define SERVICE_ERROR_IGNORE           0x00000000
#define SERVICE_ERROR_NORMAL           0x00000001
#define SERVICE_ERROR_SEVERE           0x00000002
#define SERVICE_ERROR_CRITICAL         0x00000003

typedef enum _CM_SERVICE_NODE_TYPE {
    DriverType               = SERVICE_KERNEL_DRIVER,
    FileSystemType           = SERVICE_FILE_SYSTEM_DRIVER,
    Win32ServiceOwnProcess   = SERVICE_WIN32_OWN_PROCESS,
    Win32ServiceShareProcess = SERVICE_WIN32_SHARE_PROCESS,
    AdapterType              = SERVICE_ADAPTER,
    RecognizerType           = SERVICE_RECOGNIZER_DRIVER
} SERVICE_NODE_TYPE;

typedef enum _CM_SERVICE_LOAD_TYPE {
    BootLoad    = SERVICE_BOOT_START,
    SystemLoad  = SERVICE_SYSTEM_START,
    AutoLoad    = SERVICE_AUTO_START,
    DemandLoad  = SERVICE_DEMAND_START,
    DisableLoad = SERVICE_DISABLED
} SERVICE_LOAD_TYPE;

typedef enum _CM_ERROR_CONTROL_TYPE {
    IgnoreError   = SERVICE_ERROR_IGNORE,
    NormalError   = SERVICE_ERROR_NORMAL,
    SevereError   = SERVICE_ERROR_SEVERE,
    CriticalError = SERVICE_ERROR_CRITICAL
} SERVICE_ERROR_TYPE;

//
// Boot driver list entry (PRIVATE/NTOS/INC/CM.H:72). CmpFindDrivers/
// CmpAddDriverToList allocate these and link them onto BlLoaderBlock->
// BootDriverListHead; the loader then loads each driver. Uses only types we
// already have (LIST_ENTRY, UNICODE_STRING, PLDR_DATA_TABLE_ENTRY).
//
typedef struct _BOOT_DRIVER_LIST_ENTRY {
    LIST_ENTRY Link;
    UNICODE_STRING FilePath;
    UNICODE_STRING RegistryPath;
    PLDR_DATA_TABLE_ENTRY LdrEntry;
} BOOT_DRIVER_LIST_ENTRY, *PBOOT_DRIVER_LIST_ENTRY;

//
// More opaque runtime types referenced only in cmp.h prototypes the boot subset
// never calls (object/security/APC API). ACCESS_MASK/SECURITY_INFORMATION are
// real (plain ULONG); the rest are pointer opaques.
//
typedef ULONG ACCESS_MASK;
typedef ULONG SECURITY_INFORMATION, *PSECURITY_INFORMATION;
typedef PVOID PGENERIC_MAPPING;
typedef PVOID PKNORMAL_ROUTINE;
typedef struct _CM_FULL_RESOURCE_DESCRIPTOR *PCM_FULL_RESOURCE_DESCRIPTOR;  // hardware-tree only (cmdat.c global)
typedef enum _SECURITY_OPERATION_CODE {
    SetSecurityDescriptor,
    QuerySecurityDescriptor,
    DeleteSecurityDescriptor,
    AssignSecurityDescriptor
} SECURITY_OPERATION_CODE;

//
// Bus interface type (PUBLIC/SDK/INC/NTIOAPI.H:1607, verbatim). cmdat.c indexes a
// bus-name table by these for hardware-tree building (not the boot driver scan).
//
typedef enum _INTERFACE_TYPE {
    Internal,
    Isa,
    Eisa,
    MicroChannel,
    TurboChannel,
    PCIBus,
    VMEBus,
    NuBus,
    PCMCIABus,
    CBus,
    MPIBus,
    MPSABus,
    MaximumInterfaceType
} INTERFACE_TYPE, *PINTERFACE_TYPE;

//
// Registry value types (PUBLIC/SDK/INC/WINNT.H). CmpFindDrivers checks the type
// of the values it reads (REG_DWORD for Start/Type, REG_MULTI_SZ for group lists).
//
#define REG_NONE                    0
#define REG_SZ                      1
#define REG_EXPAND_SZ               2
#define REG_BINARY                  3
#define REG_DWORD                   4
#define REG_DWORD_LITTLE_ENDIAN     4
#define REG_DWORD_BIG_ENDIAN        5
#define REG_LINK                    6
#define REG_MULTI_SZ                7

//
// HBLOCK_SIZE == PAGE_SIZE in the hive code; the CM files do not include bldr.h
// (which also defines PAGE_SIZE for the loader), so supply it here.
//
#ifndef PAGE_SIZE
#define PAGE_SIZE 0x1000
#endif

#endif // _ARM_CMSHIM_
