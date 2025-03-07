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
#include "SfsDirectory.h"
#include "SfsFile.h"
#include "VolumeFormat.h"
#include <dispatcher/Lock.h>
#include <dispatcher/SELock.h>
#include <filesystem/FSUtilities.h>


// SerenaFS Locking:
//
// seLock:         provides exclusion for mount, unmount and acquire-root-node
// allocationLock: implements atomic block allocation and deallocation
final_class_ivars(SerenaFS, ContainerFilesystem,
    SELock                  seLock;
    Lock                    moveLock;   // To make the move operation atomic
    
    SfsAllocator            blockAllocator;
    
    uint32_t                blockShift;
    uint32_t                blockMask;
    size_t                  indirectBlockEntryCount;        // Number of block pointers in an indirect block

    LogicalBlockAddress     lbaRootDir;                     // Root directory LBA (This is the inode id at the same time)

    struct {
        unsigned int    isMounted:1;    // true while mounted; false if not mounted
        unsigned int    isReadOnly:1;
        unsigned int    isAccessUpdateOnReadEnabled:1;  // true if updates to the access-date on read operations are enabled
        unsigned int    reserved:29;
    }                       mountFlags; // Flags that remain constant as long as the FS is mounted
);


extern errno_t SerenaFS_createNode(SerenaFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, sfs_insertion_hint_t* _Nullable pDirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t SerenaFS_onAcquireNode(SerenaFSRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t SerenaFS_onWritebackNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode);

extern errno_t SerenaFS_acquireRootDirectory(SerenaFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir);
extern errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);
extern errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, uid_t uid, gid_t gid, MutablePathComponent* _Nonnull pName);

#endif /* SerenaFSPriv_h */
