//
//  KernFSPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef KernFSPriv_h
#define KernFSPriv_h

#include "KernFS.h"
#include "KfsNode.h"
#include <ext/hash.h>
#include <ext/queue.h>
#include <filesystem/FSUtilities.h>
#include <sched/mtx.h>


//
// KernFS
//

#define IN_HASH_CHAINS_COUNT    8
#define IN_HASH_INDEX(__id)     (hash_scalar(__id) & (IN_HASH_CHAINS_COUNT-1))

#define KfsNodeFromHashChainPointer(__ptr) \
(KfsNodeRef) (((uint8_t*)__ptr) - offsetof(struct KfsNode, inChain))


// KernFS
//
// Inodes
//
// KernFS stores inodes (KfsDevice, KfsDirectory) internally. These nodes are
// stored in the 'inOwned' hash table and are accessed by their inode number.
// An Inode stays alive as long as its useCount and linkCount fields are both
// > 0. The useCount is managed by the acquire/relinquish APIs per usual and the
// linkCount effectively represents the internal reference count of an inode.
// Managing inodes means:
// - createNode: create the inode with linkCount == 1 and add it to 'inOwned'
// - onAcquireNode: increment useCount.
// - onWritebackNode: do nothing
// - onRelinquishNode: delete the node from 'inOwned' if linkCount == 0; do nothing otherwise
final_class_ivars(KernFS, Filesystem,
    mtx_t           inOwnedLock;
    List* _Nonnull  inOwned;            // <KfsNode>
    ino_t           nextAvailableInodeId;
);


ino_t KernFS_GetNextAvailableInodeId(KernFSRef _Nonnull _Locked self);

extern void _KernFS_AddInode(KernFSRef _Nonnull self, KfsNodeRef _Nonnull ip);
extern void _KernFS_DestroyInode(KernFSRef _Nonnull self, KfsNodeRef _Nonnull ip);
extern KfsNodeRef _Nullable _KernFS_GetInode(KernFSRef _Nonnull self, ino_t id);

extern errno_t KernFS_createNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, uid_t uid, gid_t gid, mode_t mode, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t KernFS_onAcquireNode(KernFSRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode);
extern errno_t KernFS_onWritebackNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode);
extern void KernFS_onRelinquishNode(KernFSRef _Nonnull self, InodeRef _Nonnull pNode);

extern errno_t KernFS_acquireNodeForName(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);
extern errno_t KernFS_getNameOfNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, MutablePathComponent* _Nonnull pName);

#endif /* KernFSPriv_h */
