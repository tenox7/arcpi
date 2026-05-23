#ifndef _FSBOOT_SHIM_
#define _FSBOOT_SHIM_

//
// Filesystem context types for the ported boot-lib (BLIO.C; later FATBOOT.C).
//
// Each FS defines two context types: a *_STRUCTURE_CONTEXT (per-volume, the temp
// globals in blio.c) and a *_FILE_CONTEXT (per-open-file, a member of the
// BL_FILE_TABLE union in bootlib.h). blio.c only takes the address of the
// structure contexts (opaque PVOID), and never touches the file contexts itself,
// so for now all ten are opaque placeholders sized so an accidental access stays
// in bounds. This port implements only FAT (NT on the Pi boots a FAT system
// partition); HPFS/NTFS/CDFS/OFS stay placeholders and their detectors (declared
// in bldr.h) are stubbed to NULL.
//
// Later: when FATBOOT.C is ported, FAT_STRUCTURE_CONTEXT and FAT_FILE_CONTEXT must
// become the real types from BOOT/INC/FATBOOT.H (FATBOOT.C dereferences them).
// Replace the two FAT placeholders then and recompile blio.c + fatboot.c together
// so the shared BlFileTable union and FatStructureTemp match on both sides.
//
#define FS_CTX_PLACEHOLDER_BYTES 512

typedef struct _FAT_STRUCTURE_CONTEXT  { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } FAT_STRUCTURE_CONTEXT,  *PFAT_STRUCTURE_CONTEXT;
typedef struct _HPFS_STRUCTURE_CONTEXT { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } HPFS_STRUCTURE_CONTEXT, *PHPFS_STRUCTURE_CONTEXT;
typedef struct _NTFS_STRUCTURE_CONTEXT { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } NTFS_STRUCTURE_CONTEXT, *PNTFS_STRUCTURE_CONTEXT;
typedef struct _CDFS_STRUCTURE_CONTEXT { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } CDFS_STRUCTURE_CONTEXT, *PCDFS_STRUCTURE_CONTEXT;
typedef struct _OFS_STRUCTURE_CONTEXT  { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } OFS_STRUCTURE_CONTEXT,  *POFS_STRUCTURE_CONTEXT;

typedef struct _FAT_FILE_CONTEXT  { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } FAT_FILE_CONTEXT,  *PFAT_FILE_CONTEXT;
typedef struct _HPFS_FILE_CONTEXT { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } HPFS_FILE_CONTEXT, *PHPFS_FILE_CONTEXT;
typedef struct _NTFS_FILE_CONTEXT { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } NTFS_FILE_CONTEXT, *PNTFS_FILE_CONTEXT;
typedef struct _CDFS_FILE_CONTEXT { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } CDFS_FILE_CONTEXT, *PCDFS_FILE_CONTEXT;
typedef struct _OFS_FILE_CONTEXT  { UCHAR Opaque[FS_CTX_PLACEHOLDER_BYTES]; } OFS_FILE_CONTEXT,  *POFS_FILE_CONTEXT;

//
// FS init prototypes BlIoInitialize calls. (The Is*FileStructure detectors are
// already declared in bldr.h.) Stubbed in loader/arm/fsstub.c for now: succeed
// silently, registering no filesystem. FAT graduates to the real FATBOOT.C later.
//
ARC_STATUS FatInitialize(VOID);
ARC_STATUS HpfsInitialize(VOID);
ARC_STATUS NtfsInitialize(VOID);
ARC_STATUS CdfsInitialize(VOID);

#endif // _FSBOOT_SHIM_
