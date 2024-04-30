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
#include "VolumeFormat.h"
#include <dispatcher/Lock.h>
#include <dispatcher/SELock.h>
#include <driver/MonotonicClock.h>


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

// Points to a directory entry inside a disk block
typedef struct SFSDirectoryEntryPointer {
    LogicalBlockAddress     lba;            // LBA of the disk block that holds the directory entry
    size_t                  blockOffset;    // Byte offset to the directory entry relative to the disk block start
    FileOffset              fileOffset;     // Byte offset relative to the start of the directory file
} SFSDirectoryEntryPointer;


//
// Inode Extensions
//

#define Inode_GetBlockMap(__self) \
    Inode_GetRefConAs(__self, SFSBlockMap*)


//
// SerenaFS
//

// SerenaFS Locking:
//
// seLock:         provides exclusion for mount, unmount and acquire-root-node
// allocationLock: implements atomic block allocation and deallocation
final_class_ivars(SerenaFS, Filesystem,
    SELock                  seLock;
    struct {
        unsigned int    isMounted:1;    // true while mounted; false if not mounted
        unsigned int    reserved:31;
    }                       flags;

    DiskDriverRef _Nullable diskDriver;

    Lock                    allocationLock;                 // Protects all block allocation related state    
    LogicalBlockAddress     allocationBitmapLba;            // Info for writing the allocation bitmap back to disk
    LogicalBlockCount       allocationBitmapBlockCount;     // -"-
    uint8_t* _Nullable      allocationBitmap;
    size_t                  allocationBitmapByteSize;
    uint32_t                volumeBlockCount;

    LogicalBlockAddress     rootDirLba;                     // Root directory LBA (This is the inode id at the same time)
    FilePermissions         fsPermissions;                  // Disk-wide permissions as calculated at mount time

    uint8_t                 tmpBlock[kSFSBlockSize];
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
static errno_t SerenaFS_GetLogicalBlockAddressForFileBlockAddress(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, int fba, SFSBlockMode mode, LogicalBlockAddress* _Nonnull pOutLba);
static void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length);
static errno_t SerenaFS_InsertDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, const PathComponent* _Nonnull pName, InodeId id, SFSDirectoryEntryPointer* _Nullable pEmptyPtr);

#endif /* SerenaFSPriv_h */
