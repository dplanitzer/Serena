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
#include "DfsNode.h"
#include <dispatcher/Lock.h>
#include <dispatcher/SELock.h>
#include <filesystem/FSUtilities.h>
#include <klib/Types.h>
#include <klib/Hash.h>
#include <klib/List.h>


//
// DevFS
//

#define IN_HASH_CHAINS_COUNT    8
#define IN_HASH_INDEX(__id)     (hash_scalar(__id) & (IN_HASH_CHAINS_COUNT-1))

#define DfsNodeFromHashChainPointer(__ptr) \
(DfsNodeRef) (((uint8_t*)__ptr) - offsetof(struct DfsNode, inChain))


// DevFS Locking:
//
// seLock:  provides exclusion for mount, unmount and acquire-root-node
//
// Inodes
//
// DevFS stores inodes (DfsDevice, DfsDirectory) internally. These nodes are
// stored in the 'inOwned' hash table and are accessed by their inode number.
// An Inode stays alive as long as its useCount and linkCount fields are both
// > 0. The useCount is managed by the acquire/relinquish APIs per usual and the
// linkCount effectively represents the internal reference count of an inode.
// Managing inodes means:
// - createNode: create the inode with linkCount == 1 and add it to 'inOwned'
// - onAcquireNode: increment useCount.
// - onWritebackNode: do nothing
// - onRelinquishNode: delete the node from 'inOwned' if linkCount == 0; do nothing otherwise
final_class_ivars(DevFS, Filesystem,
    SELock          seLock;
    List* _Nonnull  inOwned;            // <DfsNode>
    ino_t           rootDirInodeId;
    ino_t           nextAvailableInodeId;
    struct {
        unsigned int    isMounted:1;
        unsigned int    reserved: 31;
    }               flags;
);


ino_t DevFS_GetNextAvailableInodeId(DevFSRef _Nonnull _Locked self);

extern void _DevFS_AddInode(DevFSRef _Nonnull _Locked self, DfsNodeRef _Nonnull ip);
extern void _DevFS_DestroyInode(DevFSRef _Nonnull _Locked self, DfsNodeRef _Nonnull ip);
extern DfsNodeRef _Nullable _DevFS_GetInode(DevFSRef _Nonnull _Locked self, ino_t id);

extern errno_t DevFS_createNode(DevFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t DevFS_onAcquireNode(DevFSRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t DevFS_onWritebackNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode);
extern void DevFS_onRelinquishNode(DevFSRef _Nonnull self, InodeRef _Nonnull pNode);

extern errno_t DevFS_InsertDirectoryEntry(DevFSRef _Nonnull _Locked self, DfsDirectoryRef _Nonnull _Locked dir, DfsNodeRef _Nonnull pChildNode, const PathComponent* _Nonnull name);

extern errno_t DevFS_acquireRootDirectory(DevFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir);
extern errno_t DevFS_acquireNodeForName(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);
extern errno_t DevFS_getNameOfNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, uid_t uid, gid_t gid, MutablePathComponent* _Nonnull pName);

#endif /* DevFSPriv_h */
