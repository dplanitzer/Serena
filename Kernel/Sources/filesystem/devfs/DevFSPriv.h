//
//  DevFSPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DevFSPriv_h
#define DevFSPriv_h

#include "DevFS.h"
#include "DfsItem.h"
#include <dispatcher/Lock.h>
#include <dispatcher/SELock.h>
#include <filesystem/FSUtilities.h>
#include <klib/Types.h>
#include <klib/List.h>


//
// DevFS
//

#define INID_HASH_CHAINS_COUNT  8
#define INID_HASH_CODE(__id)    (__id)
#define INID_HASH_INDEX(__id)   (INID_HASH_CODE(__id) & (INID_HASH_CHAINS_COUNT-1))


// DevFS Locking:
//
// seLock:  provides exclusion for mount, unmount and acquire-root-node
final_class_ivars(DevFS, Filesystem,
    SELock      seLock;
    List        inidChains[INID_HASH_CHAINS_COUNT];
    InodeId     rootDirInodeId;
    InodeId     nextAvailableInodeId;
    struct {
        unsigned int    isMounted:1;
        unsigned int    reserved: 31;
    }           flags;
);


InodeId DevFS_GetNextAvailableInodeId(DevFSRef _Nonnull _Locked self);

void DevFS_AddItem(DevFSRef _Nonnull _Locked self, DfsItem* _Nonnull item);
void DevFS_RemoveItem(DevFSRef _Nonnull _Locked self, InodeId inid);
DfsItem* _Nullable DevFS_GetItem(DevFSRef _Nonnull _Locked self, InodeId inid);

extern errno_t DevFS_createNode(DevFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, UserId uid, GroupId gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t DevFS_onReadNodeFromDisk(DevFSRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t DevFS_onWriteNodeToDisk(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode);
extern void DevFS_onRemoveNodeFromDisk(DevFSRef _Nonnull self, InodeRef _Nonnull pNode);

extern errno_t DevFS_InsertDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId inid, const PathComponent* _Nonnull pName);
extern errno_t DevFS_RemoveDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId idToRemove);

extern errno_t DevFS_acquireRootDirectory(DevFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir);
extern errno_t DevFS_acquireNodeForName(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, UserId uid, GroupId gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);
extern errno_t DevFS_getNameOfNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, UserId uid, GroupId gid, MutablePathComponent* _Nonnull pName);

#endif /* DevFSPriv_h */
