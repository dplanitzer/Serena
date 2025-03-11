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

#define IN_CACHED_HASH_CHAINS_COUNT 16
#define IN_CACHED_HASH_CHAINS_MASK  (IN_CACHED_HASH_CHAINS_COUNT - 1)

#define IN_CACHED_HASH_INDEX(__inid) \
(hash_scalar(__inid) & IN_CACHED_HASH_CHAINS_MASK)

#define InodeFromHashChainPointer(__ptr) \
(InodeRef) (((uint8_t*)__ptr) - offsetof(struct Inode, sibling))


#define IN_READING_HASH_CHAINS_COUNT 4
#define IN_READING_HASH_CHAINS_MASK  (IN_READING_HASH_CHAINS_COUNT - 1)

#define IN_READING_HASH_INDEX(__inid) \
(hash_scalar(__inid) & IN_READING_HASH_CHAINS_MASK)

#define MAX_CACHED_RDNODES  4


typedef struct RDnode {
    ListNode    sibling;
    ino_t       id;
} RDnode;


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
    try(FSAllocateCleared(sizeof(List) * IN_CACHED_HASH_CHAINS_COUNT, (void**)&self->inCached));
    try(FSAllocateCleared(sizeof(List) * IN_READING_HASH_CHAINS_COUNT, (void**)&self->inReading));
    
    self->fsid = Filesystem_GetNextAvailableId();
    ConditionVariable_Init(&self->inCondVar);
    Lock_Init(&self->inLock);
    List_Init(&self->inReadingCache);
    self->state = kFilesystemState_Idle;

    *pOutFileSys = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutFileSys = NULL;
    return err;
}

void Filesystem_deinit(FilesystemRef _Nonnull self)
{
    if (self->state != kFilesystemState_Stopped) {
        // This should never happen because a filesystem can not be destroyed
        // as long as inodes are still alive
        abort();
    }

    FSDeallocate(self->inCached);
    self->inCached = NULL;

    FSDeallocate(self->inReading);
    self->inReading = NULL;

    List_Deinit(&self->inReadingCache);
    Lock_Deinit(&self->inLock);
    ConditionVariable_Deinit(&self->inCondVar);
}

static errno_t _Filesystem_PrepReadingNode(FilesystemRef _Nonnull self, ino_t id, RDnode* _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    RDnode* rdp;

    if (self->inReadingCacheCount > 0) {
        rdp = (RDnode*)List_RemoveFirst(&self->inReadingCache);
        self->inReadingCacheCount--;
    }
    else {
        err = FSAllocateCleared(sizeof(RDnode), (void**)&rdp);
    }

    if (err == EOK) {
        rdp->id = id;
        List_InsertBeforeFirst(&self->inReading[IN_READING_HASH_INDEX(id)], &rdp->sibling);
        self->inReadingCount++;
    }

    *pOutNode = rdp;
    return err;
}

static void _Filesystem_FinReadingNode(FilesystemRef _Nonnull self, RDnode* _Nonnull rdp)
{
    List_Remove(&self->inReading[IN_READING_HASH_INDEX(rdp->id)], &rdp->sibling);
    self->inReadingCount--;

    if (self->inReadingCacheCount < MAX_CACHED_RDNODES) {
        List_InsertBeforeFirst(&self->inReadingCache, &rdp->sibling);
        self->inReadingCacheCount++;
    }
    else {
        FSDeallocate(rdp);
    }
}

errno_t Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef ip = NULL;
    bool doBroadcast = false;

    Lock_Lock(&self->inLock);

    // Don't return inodes if we aren't active
    if (self->state != kFilesystemState_Active) {
        throw(ENXIO);
    }


retry:
    // Check whether we already got the inode cached
    List_ForEach(&self->inCached[IN_CACHED_HASH_INDEX(id)], struct Inode,
        InodeRef curNode = InodeFromHashChainPointer(pCurNode);

        if (Inode_GetId(curNode) == id) {
            ip = curNode;
            break;
        }
    );


    if (ip == NULL) {
        // Check whether someone else already kicked off the process of reading
        // the inode off the disk. We'll wait if that's the case.
        bool isReading = false;

        List_ForEach(&self->inReading[IN_READING_HASH_INDEX(id)], struct RDnode,
            if (((RDnode*)pCurNode)->id == id) {
                isReading = true;
                break;
            }
        );

        if (isReading) {
            self->inReadingWaiterCount++;
            try_bang(ConditionVariable_Wait(&self->inCondVar, &self->inLock, kTimeInterval_Infinity));
            self->inReadingWaiterCount--;
            goto retry;
        }
    }



    if (ip) {
        // If the node is cached or in the process of being written back to disk -> return it
        // If the node is in the process of being deleted -> return NULL (not found)
        //
        // If node is being written back: we return it right away. The node is locked
        // until the write back is done. Thus the caller will block on the inode lock
        // when it tries to access it.
        if (ip->state == kInodeState_Deleting) {
            ip = NULL;
            err = ENOENT;
        }
    }
    else {
        // The inode doesn't exist yet. Need to trigger a read from the disk. We'll
        // create a (temporary) RDnode to track the fact that the inode is in the
        // process of reading off the disk. The actual read is done outside of the
        // inode management locking scope so that we don't block other folks from
        // doing inode management while we're busy hitting the disk.
        RDnode* rdp = NULL;
        
        err = _Filesystem_PrepReadingNode(self, id, &rdp);
        if (err == EOK) {
            Lock_Unlock(&self->inLock);
            err = Filesystem_OnAcquireNode(self, id, &ip);
            Lock_Lock(&self->inLock);

            _Filesystem_FinReadingNode(self, rdp);
        }

        if (err == EOK) {
            List_InsertBeforeFirst(&self->inCached[IN_CACHED_HASH_INDEX(id)], &ip->sibling);
            self->inCachedCount++;
            ip->state = kInodeState_Cached;
        }

        // Wake waiters no matter whether reading has succeeded or failed
        doBroadcast = (self->inReadingWaiterCount > 0) ? true : false;
    }

    ip->useCount++;

catch:
    if (doBroadcast) {
        ConditionVariable_BroadcastAndUnlock(&self->inCondVar, &self->inLock);
    }
    else {
        Lock_Unlock(&self->inLock);
    }
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
        List_Remove(&self->inCached[IN_CACHED_HASH_INDEX(Inode_GetId(pNode))], &pNode->sibling);
        self->inCachedCount--;
        doDestroy = true;
    }

    Lock_Unlock(&self->inLock);

    if (doDestroy) {
        Filesystem_OnRelinquishNode(self, pNode);
    }

    return err;
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


errno_t Filesystem_Start(FilesystemRef _Nonnull self, const void* _Nonnull params, ssize_t paramsSize)
{
    decl_try_err();

    Lock_Lock(&self->inLock);
    switch (self->state) {
        case kFilesystemState_Active:   throw(EBUSY); break;
        case kFilesystemState_Stopped:  throw(ENXIO); break;
        default: break;
    }

    err = Filesystem_OnStart(self, params, paramsSize);
    if (err == EOK) {
        self->state = kFilesystemState_Active;
    }

catch:
    Lock_Unlock(&self->inLock);
    return err;
}

errno_t Filesystem_onStart(FilesystemRef _Nonnull self, const void* _Nonnull params, ssize_t paramsSize)
{
    return EOK;
}

bool Filesystem_CanStop(FilesystemRef _Nonnull self)
{
    Lock_Lock(&self->inLock);
    const bool ok = (self->inCachedCount == 0 && self->inReadingCount == 0) ? true : false;
    Lock_Unlock(&self->inLock);
    return ok;
}

errno_t Filesystem_Stop(FilesystemRef _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->inLock);
    if (self->state != kFilesystemState_Active) {
        throw(ENXIO);
    }
    if (self->inCachedCount > 0 || self->inReadingCount > 0) {
        throw(EBUSY);
    }

    err = Filesystem_OnStop(self);
    self->state = kFilesystemState_Stopped;

catch:
    Lock_Unlock(&self->inLock);
    return err;
}

errno_t Filesystem_onStop(FilesystemRef _Nonnull self)
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
func_def(onStart, Filesystem)
func_def(onStop, Filesystem)
func_def(acquireRootDirectory, Filesystem)
func_def(acquireNodeForName, Filesystem)
func_def(getNameOfNode, Filesystem)
func_def(createNode, Filesystem)
func_def(isReadOnly, Filesystem)
func_def(unlink, Filesystem)
func_def(move, Filesystem)
func_def(rename, Filesystem)
);
