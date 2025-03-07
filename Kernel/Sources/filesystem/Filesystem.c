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

    if (ip == NULL) {
        try(Filesystem_OnAcquireNode(self, id, &ip));

        List_InsertBeforeFirst(&self->inHashTable[hashIdx], &ip->sibling);
        self->inCount++;
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

    if ((pNode->useCount == 1 && pNode->linkCount == 0) || Inode_IsModified(pNode)) {
        if (!Filesystem_IsReadOnly(self)) {
            err = Filesystem_OnWritebackNode(self, pNode);
        }
        Inode_ClearModified(pNode);
    }

    assert(pNode->useCount > 0);
    pNode->useCount--;
    if (pNode->useCount == 0) {
        const size_t hashIdx = IN_HASH_INDEX(Inode_GetId(pNode));

        self->inCount--;
        List_Remove(&self->inHashTable[hashIdx], &pNode->sibling);
        Filesystem_OnRelinquishNode(self, pNode);
    }

    Lock_Unlock(&self->inLock);

    return err;
}

bool Filesystem_CanUnmount(FilesystemRef _Nonnull self)
{
    Lock_Lock(&self->inLock);
    const bool ok = (self->inCount > 0) ? true : false;
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
