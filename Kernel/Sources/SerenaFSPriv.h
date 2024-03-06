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

#define kSFSMaxFilenameLength               28
#define kSFSBlockSizeShift                  9
#define kSFSBlockSize                       (1 << kSFSBlockSizeShift)
#define kSFSBlockSizeMask                   (kSFSBlockSize - 1)
#define kSFSDirectoryEntriesPerBlock        (kSFSBlockSize / sizeof(SFSDirectoryEntry))
#define kSFSDirectoryEntriesPerBlockMask    (kSFSDirectoryEntriesPerBlock - 1)
#define kSFSMaxDirectDataBlockPointers      114


//
// Serena FS On-Disk Format
//

//
// Directory Entries
//

// Directory file organisation:
// [0] "."
// [1] ".."
// [2] userEntry0
// .
// [n] userEntryN-1
// This should be mod(SFSDiskBlockSize, SFSDirectoryEntrySize) == 0
typedef struct SFSDirectoryEntry {
    InodeId     id;
    char        filename[kSFSMaxFilenameLength];    // if strlen(filename) < kSFSMaxFilenameLength -> 0 terminated
} SFSDirectoryEntry;


//
// Inodes
//

typedef struct SFSBlockMap {
    char* _Nullable p[kSFSMaxDirectDataBlockPointers];
} SFSBlockMap;


// NOTE: disk nodes own the data blocks of a file/directory. Inodes are set up
// with a pointer to the disk node block map. So inodes manipulate the block map
// directly instead of copying it back and forth. That's okay because the inode
// lock effectively protects the disk node sitting behind the inode. 
typedef struct SFSInode {
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
    SFSBlockMap         blockMap;
} SFSInode;
typedef SFSInode* SFSInodeRef;



//
// Directories
//

enum SFSDirectoryQueryKind {
    kSFSDirectoryQuery_PathComponent,
    kSFSDirectoryQuery_InodeId
};

typedef struct SFSDirectoryQuery {
    int     kind;
    union _u {
        const PathComponent* _Nonnull   pc;
        InodeId                         id;
    }       u;
} SFSDirectoryQuery;


//
// Inode Extensions
//

#define Inode_GetBlockMap(__self) \
    Inode_GetRefConAs(__self, SFSBlockMap*)


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
    char                emptyBlock[kSFSBlockSize];  // Block filled with zeros used by the read() function if there's no disk block with data
);

typedef ssize_t (*SFSReadCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);
typedef void (*SFSWriteCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);

typedef enum SFSBlockMode {
    kSFSBlockMode_Read = 0,
    kSFSBlockMode_Write
} SFSBlockMode;


static InodeId SerenaFS_GetNextAvailableInodeId_Locked(SerenaFSRef _Nonnull self);
static errno_t SerenaFS_FormatWithEmptyFilesystem(SerenaFSRef _Nonnull self);
static errno_t SerenaFS_CreateDirectoryDiskNode(SerenaFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId);
static void SerenaFS_DestroyDiskNode(SerenaFSRef _Nonnull self, SFSInodeRef _Nullable pDiskNode);
static errno_t SerenaFS_GetDiskBlockForBlockIndex(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, int blockIdx, SFSBlockMode mode, char* _Nullable * _Nonnull pOutDiskBlock);
static void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length);

#endif /* SerenaFSPriv_h */
