//
//  RamFSPriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 12/7/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef RamFSPriv_h
#define RamFSPriv_h

#include "RamFS.h"
#include "ConditionVariable.h"
#include "Lock.h"

#define kMaxFilenameLength      28
#define kRamBlockSizeShift      9
#define kRamBlockSize           (1 << 9)
#define kRamBlockSizeMask       (kRamBlockSize - 1)
#define kRamDirectoryEntriesPerBlock        (kRamBlockSize / sizeof(RamDirectoryEntry))
#define kRamDirectoryEntriesPerBlockMask    (kRamDirectoryEntriesPerBlock - 1)
#define kMaxDirectDataBlockPointers 114


//
// RamFS Directories
//

// Directory content organisation:
// [0] "."
// [1] ".."
// [2] userEntry0
// .
// [n] userEntryN-1
// This should be mod(RamDiskBlockSize, RamDirectoryEntrySize) == 0
typedef struct _RamDirectoryEntry {
    InodeId     id;
    Character   filename[kMaxFilenameLength];
} RamDirectoryEntry;


enum RamDirectoryQueryKind {
    kDirectoryQuery_PathComponent,
    kDirectoryQuery_InodeId
};

typedef struct RamDirectoryQuery {
    Int     kind;
    union _u {
        const PathComponent* _Nonnull   pc;
        InodeId                         id;
    }       u;
} RamDirectoryQuery;


//
// RamFS Disk Nodes
//

typedef struct _RamBlockMap {
    Byte* _Nullable p[kMaxDirectDataBlockPointers];
} RamBlockMap;


// NOTE: disk nodes own the data blocks of a file/directory. Inodes are set up
// with a pointer to the disk node block map. So inodes manipulate the block map
// directly instead of copying it back and forth. That's okay because the inode
// lock effectively protects the disk node sitting behind the inode. 
typedef struct _RamDiskNode {
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    TimeInterval        statusChangeTime;
    FileOffset          size;
    InodeId             id;
    UserId              uid;
    GroupId             gid;
    FilePermissions     permissions;
    Int                 linkCount;
    FileType            type;
    RamBlockMap         blockMap;
} RamDiskNode;
typedef RamDiskNode* RamDiskNodeRef;


//
// RamFS Inode Block Map
//

#define Inode_GetBlockMap(__self) \
    Inode_GetRefConAs(__self, RamBlockMap*)


//
// RamFS
//

CLASS_IVARS(RamFS, Filesystem,
    Lock                lock;           // Shared between filesystem proper and inodes
    User                rootDirUser;    // User we should use for the root directory
    ConditionVariable   notifier;
    InodeId             rootDirId;
    PointerArray        dnodes;         // Array<RamDiskNodeRef>
    Int                 nextAvailableInodeId;
    Bool                isMounted;
    Bool                isReadOnly;     // true if mounted read-only; false if mounted read-write
    Byte                emptyBlock[kRamBlockSize];  // Block filled with zeros used by the read() function if there's no disk block with data
);

typedef ByteCount (*RamReadCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ByteCount n);
typedef void (*RamWriteCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ByteCount n);

typedef enum _BlockAccessMode {
    kBlock_Read = 0,
    kBlock_Write
} BlockAccessMode;


static InodeId RamFS_GetNextAvailableInodeId_Locked(RamFSRef _Nonnull self);
static ErrorCode RamFS_FormatWithEmptyFilesystem(RamFSRef _Nonnull self);
static ErrorCode RamFS_CreateDirectoryDiskNode(RamFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId);
static void RamFS_DestroyDiskNode(RamFSRef _Nonnull self, RamDiskNodeRef _Nullable pDiskNode);
static ErrorCode RamFS_GetDiskBlockForBlockIndex(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, Int blockIdx, BlockAccessMode mode, Byte* _Nullable * _Nonnull pOutDiskBlock);
static void RamFS_xTruncateFile(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length);

#endif /* RamFSPriv_h */
