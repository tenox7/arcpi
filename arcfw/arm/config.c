//
// arcfw/arm/config.c - synthesized ARC firmware configuration tree.
//
// The Raspberry Pi has no ARC ROM, so - exactly as the x86 ARCEMUL does for a PC
// (BOOT/LIB/I386/ARCEMUL.C: FwConfigurationTree + FwGetChild/FwGetPeer/AEGetParent/
// AEGetConfigurationData) - we synthesize the configuration component tree the
// firmware would expose and serve it through the GetChild/GetPeer/GetParent/GetData
// firmware-vector slots. The portable BlConfigurationInitialize (arcfw/ported/blconfig.c)
// then copies it into the loader heap and sets BlLoaderBlock->ConfigurationRoot; after
// that BlGetArcDiskInformation and KeFindConfigurationEntry walk it like any ARC machine.
//
// Minimal tree for one bootable ramdisk:
//   System(arc) -> MultiFunctionAdapter(multi) -> DiskController(disk) -> DiskPeripheral(rdisk)
// BlGetPathnameFromComponent renders the disk as "multi(0)disk(0)rdisk(0)", which
// BlGetArcDiskInformation opens as partition(0) (whole disk) to read the MBR signature.
// Internal nodes are CONFIGURATION_COMPONENT_DATA (Parent/Child/Sibling + embedded
// ComponentEntry); the Get* routines CONTAINING_RECORD back from &ComponentEntry, like x86.
//
#include "bldr.h"
#include "string.h"

static CONFIGURATION_COMPONENT_DATA ArcSystemNode;
static CONFIGURATION_COMPONENT_DATA ArcAdapterNode;
static CONFIGURATION_COMPONENT_DATA ArcControllerNode;
static CONFIGURATION_COMPONENT_DATA ArcDiskNode;

static PCONFIGURATION_COMPONENT_DATA ArcConfigTree;

static VOID
SetComponent(
    PCONFIGURATION_COMPONENT_DATA Node,
    CONFIGURATION_CLASS Class,
    CONFIGURATION_TYPE Type,
    ULONG Key
    )
{
    Node->Parent = NULL;
    Node->Child = NULL;
    Node->Sibling = NULL;
    Node->ConfigurationData = NULL;
    Node->ComponentEntry.Class = Class;
    Node->ComponentEntry.Type = Type;
    Node->ComponentEntry.Version = 0;
    Node->ComponentEntry.Revision = 0;
    Node->ComponentEntry.Key = Key;
    Node->ComponentEntry.AffinityMask = 0;
    Node->ComponentEntry.ConfigurationDataLength = 0;
    Node->ComponentEntry.IdentifierLength = 0;
    Node->ComponentEntry.Identifier = NULL;
}

VOID
BlArmInitializeArcConfigTree(
    VOID
    )
{
    SetComponent(&ArcSystemNode,     SystemClass,     ArcSystem,            0);
    SetComponent(&ArcAdapterNode,    AdapterClass,    MultiFunctionAdapter, 0);
    SetComponent(&ArcControllerNode, ControllerClass, DiskController,       0);
    SetComponent(&ArcDiskNode,       PeripheralClass, DiskPeripheral,       0);

    ArcSystemNode.Child = &ArcAdapterNode;

    ArcAdapterNode.Parent = &ArcSystemNode;
    ArcAdapterNode.Child = &ArcControllerNode;

    ArcControllerNode.Parent = &ArcAdapterNode;
    ArcControllerNode.Child = &ArcDiskNode;

    ArcDiskNode.Parent = &ArcControllerNode;

    ArcConfigTree = &ArcSystemNode;
}

PCONFIGURATION_COMPONENT
AEGetChild(
    IN PCONFIGURATION_COMPONENT Current
    )
{
    PCONFIGURATION_COMPONENT_DATA CurrentEntry;

    if (Current) {
        CurrentEntry = CONTAINING_RECORD(Current,
                                         CONFIGURATION_COMPONENT_DATA,
                                         ComponentEntry);
        if (CurrentEntry->Child) {
            return &CurrentEntry->Child->ComponentEntry;
        }
        return NULL;
    }

    if (ArcConfigTree) {
        return &ArcConfigTree->ComponentEntry;
    }
    return NULL;
}

PCONFIGURATION_COMPONENT
AEGetPeer(
    IN PCONFIGURATION_COMPONENT Current
    )
{
    PCONFIGURATION_COMPONENT_DATA CurrentEntry;

    if (Current) {
        CurrentEntry = CONTAINING_RECORD(Current,
                                         CONFIGURATION_COMPONENT_DATA,
                                         ComponentEntry);
        if (CurrentEntry->Sibling) {
            return &CurrentEntry->Sibling->ComponentEntry;
        }
    }
    return NULL;
}

PCONFIGURATION_COMPONENT
AEGetParent(
    IN PCONFIGURATION_COMPONENT Current
    )
{
    PCONFIGURATION_COMPONENT_DATA CurrentEntry;

    if (Current) {
        CurrentEntry = CONTAINING_RECORD(Current,
                                         CONFIGURATION_COMPONENT_DATA,
                                         ComponentEntry);
        if (CurrentEntry->Parent) {
            return &CurrentEntry->Parent->ComponentEntry;
        }
    }
    return NULL;
}

ARC_STATUS
AEGetConfigurationData(
    IN PVOID ConfigurationData,
    IN PCONFIGURATION_COMPONENT Current
    )
{
    PCONFIGURATION_COMPONENT_DATA CurrentEntry;

    if (Current) {
        CurrentEntry = CONTAINING_RECORD(Current,
                                         CONFIGURATION_COMPONENT_DATA,
                                         ComponentEntry);
        RtlMoveMemory(ConfigurationData,
                      CurrentEntry->ConfigurationData,
                      Current->ConfigurationDataLength);
        return ESUCCESS;
    }
    return EINVAL;
}
