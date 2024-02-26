//
//  SerenaFSPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/7/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef SerenaFSPriv_h
#define SerenaFSPriv_h

#include "SerenaFS.h"
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
// SerenaFS Directories
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
    char   filename[kMaxFilenameLength];
} RamDirectoryEntry;


enum RamDirectoryQueryKind {
    kDirectoryQuery_PathComponent,
    kDirectoryQuery_InodeId
};

typedef struct RamDirectoryQuery {
    int     kind;
    union _u {
        const PathComponent* _Nonnull   pc;
        InodeId                         id;
    }       u;
} RamDirectoryQuery;


//
// SerenaFS Disk Nodes
//

typedef struct _RamBlockMap {
    char* _Nullable p[kMaxDirectDataBlockPointers];
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
    int                 linkCount;
    FileType            type;
    RamBlockMap         blockMap;
} RamDiskNode;
typedef RamDiskNode* RamDiskNodeRef;


//
// SerenaFS Inode Block Map
//

#define Inode_GetBlockMap(__self) \
    Inode_GetRefConAs(__self, RamBlockMap*)


//
// SerenaFS
//

CLASS_IVARS(SerenaFS, Filesystem,
    Lock                lock;           // Shared between filesystem proper and inodes
    User                rootDirUser;    // User we should use for the root directory
    ConditionVariable   notifier;
    InodeId             rootDirId;
    PointerArray        dnodes;         // Array<RamDiskNodeRef>
    int                 nextAvailableInodeId;
    bool                isMounted;
    bool                isReadOnly;     // true if mounted read-only; false if mounted read-write
    char                emptyBlock[kRamBlockSize];  // Block filled with zeros used by the read() function if there's no disk block with data
);

typedef ssize_t (*RamReadCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);
typedef void (*RamWriteCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);

typedef enum _BlockAccessMode {
    kBlock_Read = 0,
    kBlock_Write
} BlockAccessMode;


static InodeId SerenaFS_GetNextAvailableInodeId_Locked(SerenaFSRef _Nonnull self);
static errno_t SerenaFS_FormatWithEmptyFilesystem(SerenaFSRef _Nonnull self);
static errno_t SerenaFS_CreateDirectoryDiskNode(SerenaFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId);
static void SerenaFS_DestroyDiskNode(SerenaFSRef _Nonnull self, RamDiskNodeRef _Nullable pDiskNode);
static errno_t SerenaFS_GetDiskBlockForBlockIndex(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, int blockIdx, BlockAccessMode mode, char* _Nullable * _Nonnull pOutDiskBlock);
static void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length);

#endif /* SerenaFSPriv_h */
