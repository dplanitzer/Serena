//
//  Filesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Filesystem.h"
#include "FSUtilities.h"
#include "DirectoryChannel.h"
#include "FileChannel.h"
#include <klib/Hash.h>

#define IN_HASH_CHAINS_COUNT    8
#define IN_HASH_CHAINS_MASK     (IN_HASH_CHAINS_COUNT - 1)

#define IN_HASH_INDEX(__inid) \
(hash_scalar(__inid) & IN_HASH_CHAINS_MASK)

#define InodeFromHashChainPointer(__ptr) \
(InodeRef) (((uint8_t*)__ptr) - offsetof(struct Inode, sibling))



// Returns the next available FSID.
static fsid_t Filesystem_GetNextAvailableId(void)
{
    // XXX want to:
    // XXX handle overflow (wrap around)
    // XXX make sure the generated id isn't actually in use by someone else
    static volatile AtomicInt gNextAvailableId = 0;
    return (fsid_t) AtomicInt_Increment(&gNextAvailableId);
}

errno_t Filesystem_Create(Class* pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    FilesystemRef self;

    try(Object_Create(pClass, 0, (void**)&self));
    try(FSAllocateCleared(sizeof(List) * IN_HASH_CHAINS_COUNT, (void**)&self->inHashTable));
    self->fsid = Filesystem_GetNextAvailableId();
    Lock_Init(&self->inLock);

    *pOutFileSys = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutFileSys = NULL;
    return err;
}

void Filesystem_deinit(FilesystemRef _Nonnull self)
{
    if (self->inCount > 0) {
        // This should never happen because a filesystem can not be destroyed
        // as long as inodes are still alive
        abort();
    }

    Lock_Deinit(&self->inLock);
}

errno_t Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const size_t hashIdx = IN_HASH_INDEX(id);
    InodeRef ip = NULL;

    Lock_Lock(&self->inLock);

    List_ForEach(&self->inHashTable[hashIdx], struct Inode,
        InodeRef curNode = InodeFromHashChainPointer(pCurNode);

        if (Inode_GetId(curNode) == id) {
            ip = curNode;
            break;
        }
    );

    if (ip) {
        switch (ip->state) {
            case kInodeState_Cached:
                // Just return the cached inode
                break;

            case kInodeState_Writeback:
                // Inode meta-data is in the process of being written to disk. The
                // inode is locked. We return a reference to it to the caller right
                // away. The caller will have to take the inode lock and will get
                // blocked until the node write has completed.
                break;
                
            case kInodeState_Deleting:
                // Inode is in the process of being deleted. Return NULL to the
                // caller.
                ip = NULL;
                break;

            default:
                abort();
                break;
        }
    }
    else {
        try(Filesystem_OnAcquireNode(self, id, &ip));

        List_InsertBeforeFirst(&self->inHashTable[hashIdx], &ip->sibling);
        self->inCount++;
        ip->state = kInodeState_Cached;
    }

    ip->useCount++;

catch:
    Lock_Unlock(&self->inLock);
    *pOutNode = ip;

    return err;
}

InodeRef _Nonnull _Locked Filesystem_ReacquireNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    Lock_Lock(&self->inLock);
    pNode->useCount++;
    Lock_Unlock(&self->inLock);

    return pNode;
}

errno_t Filesystem_RelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nullable pNode)
{
    decl_try_err();

    if (pNode == NULL) {
        return EOK;
    }
    
    Lock_Lock(&self->inLock);

    if (pNode->useCount == 1) {
        // We know that no one else has a reference to the inode. So taking its
        // lock here will not block us here
        Inode_Lock(pNode);

        // Update the node state
        if (pNode->linkCount == 0) {
            pNode->state = kInodeState_Deleting;
        }
        else if (Inode_IsModified(pNode)) {
            pNode->state = kInodeState_Writeback;
        }

        if (pNode->state == kInodeState_Writeback || pNode->state == kInodeState_Deleting) {
            // Drop the inode management lock. The writeback may be done
            // synchronously and may be time consuming. Don't block other processes
            // from acquiring/relinquishing inodes in the meantime 
            Lock_Unlock(&self->inLock);

            if (!Filesystem_IsReadOnly(self)) {
                err = Inode_Writeback(pNode);
            }
            else {
                err = EROFS;
            }
            
            Lock_Lock(&self->inLock);
        }
        Inode_Unlock(pNode);
    }


    // Let the filesystem destroy or retire the inode if no more references
    // exist to it. Note, that we do the writeback of a node above outside of
    // holding the inode management lock. So someone else might actually have
    // taken out a new reference on the note that we wrote back. Thus the use
    // count of that note would now be > 1 again and we won't free it here.
    if (pNode->useCount <= 0) {
        abort();
    }

    bool doDestroy = false;
    pNode->useCount--;
    if (pNode->useCount == 0) {
        List_Remove(&self->inHashTable[IN_HASH_INDEX(Inode_GetId(pNode))], &pNode->sibling);
        self->inCount--;
        doDestroy = true;
    }

    Lock_Unlock(&self->inLock);

    if (doDestroy) {
        Filesystem_OnRelinquishNode(self, pNode);
    }

    return err;
}

bool Filesystem_CanUnmount(FilesystemRef _Nonnull self)
{
    Lock_Lock(&self->inLock);
    const bool ok = (self->inCount == 0) ? true : false;
    Lock_Unlock(&self->inLock);
    return ok;
}

errno_t Filesystem_onAcquireNode(FilesystemRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode)
{
    return EIO;
}

errno_t Filesystem_onWritebackNode(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    return EIO;
}

void Filesystem_onRelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    Inode_Destroy(pNode);
}


errno_t Filesystem_start(FilesystemRef _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize)
{
    return EOK;
}

errno_t Filesystem_stop(FilesystemRef _Nonnull self)
{
    return EOK;
}


errno_t Filesystem_acquireRootDirectory(FilesystemRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    *pOutDir = NULL;
    return EIO;
}

errno_t Filesystem_acquireNodeForName(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    if (pOutNode) {
        *pOutNode = NULL;
    }
    
    if (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.') {
        return EIO;
    } else {
        return ENOENT;
    }
}

errno_t Filesystem_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, uid_t uid, gid_t gid, MutablePathComponent* _Nonnull pName)
{
    pName->count = 0;
    return EIO;
}

errno_t Filesystem_createNode(FilesystemRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    return EIO;
}

bool Filesystem_isReadOnly(FilesystemRef _Nonnull self)
{
    return true;
}

errno_t Filesystem_unlink(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked target, InodeRef _Nonnull _Locked dir)
{
    return EACCESS;
}

errno_t Filesystem_move(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    return EACCESS;
}

errno_t Filesystem_rename(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid)
{
    return EACCESS;
}


class_func_defs(Filesystem, Object,
override_func_def(deinit, Filesystem, Object)
func_def(onAcquireNode, Filesystem)
func_def(onWritebackNode, Filesystem)
func_def(onRelinquishNode, Filesystem)
func_def(start, Filesystem)
func_def(stop, Filesystem)
func_def(acquireRootDirectory, Filesystem)
func_def(acquireNodeForName, Filesystem)
func_def(getNameOfNode, Filesystem)
func_def(createNode, Filesystem)
func_def(isReadOnly, Filesystem)
func_def(unlink, Filesystem)
func_def(move, Filesystem)
func_def(rename, Filesystem)
);
