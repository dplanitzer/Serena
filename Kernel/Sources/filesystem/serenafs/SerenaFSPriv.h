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
#include "SfsAllocator.h"
#include "VolumeFormat.h"
#include <dispatcher/Lock.h>
#include <dispatcher/SELock.h>
#include <filesystem/FSUtilities.h>


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
// SerenaFS
//

// SerenaFS Locking:
//
// seLock:         provides exclusion for mount, unmount and acquire-root-node
// allocationLock: implements atomic block allocation and deallocation
final_class_ivars(SerenaFS, ContainerFilesystem,
    SELock                  seLock;
    Lock                    moveLock;   // To make the move operation atomic
    struct {
        unsigned int    isMounted:1;    // true while mounted; false if not mounted
        unsigned int    isReadOnly:1;
        unsigned int    isAccessUpdateOnReadEnabled:1;  // true if updates to the access-date on read operations are enabled
        unsigned int    reserved:29;
    }                       mountFlags; // Flags that remain constant as long as the FS is mounted

    SfsAllocator            blockAllocator;

    LogicalBlockAddress     rootDirLba;                     // Root directory LBA (This is the inode id at the same time)
);

typedef ssize_t (*SFSReadCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);
typedef void (*SFSWriteCallback)(void* _Nonnull pDst, const void* _Nonnull pSrc, ssize_t n);


extern errno_t SerenaFS_createNode(SerenaFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, SFSDirectoryEntryPointer* _Nullable pDirInsertionHint, UserId uid, GroupId gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t SerenaFS_onReadNodeFromDisk(SerenaFSRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t SerenaFS_onWriteNodeToDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode);
extern void SerenaFS_onRemoveNodeFromDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode);

extern bool DirectoryNode_IsNotEmpty(InodeRef _Nonnull _Locked self);
extern errno_t SerenaFS_InsertDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, const PathComponent* _Nonnull pName, InodeId id, const SFSDirectoryEntryPointer* _Nullable pEmptyPtr);
extern errno_t SerenaFS_RemoveDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, InodeId idToRemove);
extern errno_t SerenaFS_GetDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, const SFSDirectoryQuery* _Nonnull pQuery, SFSDirectoryEntryPointer* _Nullable pOutEmptyPtr, SFSDirectoryEntryPointer* _Nullable pOutEntryPtr, InodeId* _Nullable pOutId, MutablePathComponent* _Nullable pOutFilename);
extern errno_t SerenaFS_acquireRootDirectory(SerenaFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir);
extern errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, UserId uid, GroupId gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);
extern errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, UserId uid, GroupId gid, MutablePathComponent* _Nonnull pName);
extern errno_t SerenaFS_readDirectory(SerenaFSRef _Nonnull self, DirectoryChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

extern errno_t SerenaFS_AcquireFileBlock(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, int fba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock);
extern errno_t SerenaFS_xRead(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead);
extern errno_t SerenaFS_xWrite(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten);
extern errno_t SerenaFS_readFile(SerenaFSRef _Nonnull self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);
extern errno_t SerenaFS_writeFile(SerenaFSRef _Nonnull self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);
extern void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length);
extern errno_t SerenaFS_truncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pFile, FileOffset length);

#endif /* SerenaFSPriv_h */
