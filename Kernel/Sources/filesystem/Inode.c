//
//  Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Inode.h"
#include "DirectoryChannel.h"
#include "FileChannel.h"
#include "Filesystem.h"
#include "FSUtilities.h"
#include <security/SecurityManager.h>

typedef void (*deinit_impl_t)(void* _Nonnull self);



errno_t Inode_Create(Class* _Nonnull pClass, FilesystemRef _Nonnull pFS, ino_t id,
    mode_t mode, uid_t uid, gid_t gid, int linkCount,
    off_t size,
    const struct timespec* _Nonnull accessTime,
    const struct timespec* _Nonnull modTime,
    const struct timespec* _Nonnull statusChangeTime,
    ino_t pnid,
    InodeRef _Nullable * _Nonnull pOutNode)
{
    InodeRef self;

    const errno_t err = FSAllocateCleared(pClass->instanceSize, (void**) &self);
    if (err == EOK) {
        self->super.clazz = pClass;
        self->useCount = 0;
        self->state = kInodeState_Reading;

        mtx_init(&self->mtx);
        self->accessTime = *accessTime;
        self->modificationTime = *modTime;
        self->statusChangeTime = *statusChangeTime;
        self->size = size;
        self->filesystem = pFS;
        self->inid = id;
        self->pnid = pnid;
        self->linkCount = linkCount;
        self->mode = mode;
        self->uid = uid;
        self->gid = gid;
        self->flags = 0;
    }
    *pOutNode = self;
    return err;
}

void Inode_deinit(ObjectRef _Nonnull self)
{
}

static void _Inode_Deinit(InodeRef _Nonnull self)
{
    deinit_impl_t pPrevImpl = NULL;
    Class* pCurClass = classof(self);

    for(;;) {
        deinit_impl_t pCurImpl = (deinit_impl_t)implementationof(deinit, Inode, pCurClass);
        
        if (pCurImpl != pPrevImpl) {
            pCurImpl(self);
            pPrevImpl = pCurImpl;
        }

        if (pCurClass == class(Inode)) {
            break;
        }

        pCurClass = pCurClass->super;
    }
}

void Inode_Destroy(InodeRef _Nullable self)
{
    if (self) {
        self->filesystem = NULL;
        mtx_deinit(&self->mtx);

        _Inode_Deinit(self);
        FSDeallocate(self);
    }
}

errno_t Inode_Writeback(InodeRef _Nonnull _Locked self)
{
    const errno_t err = Filesystem_OnWritebackNode(self->filesystem, self);

    self->flags &= ~(kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged);
    return err;
}

void Inode_Unlink(InodeRef _Nonnull self)
{
    if (self->linkCount > 0) {
        self->linkCount--;
    }
}

// Returns true if the receiver and 'pOther' are the same node; false otherwise
bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther)
{
    return self->inid == pOther->inid
        && Filesystem_GetId(self->filesystem) == Filesystem_GetId(pOther->filesystem);
}

errno_t Inode_Relinquish(InodeRef _Nullable self)
{
    decl_try_err();

    if (self) {
        err = Filesystem_RelinquishNode(self->filesystem, self);
    }
    return err;
}

errno_t Inode_UnlockRelinquish(InodeRef _Nullable _Locked self)
{
    decl_try_err();

    if (self) {
        Inode_Unlock(self);
        err = Filesystem_RelinquishNode(self->filesystem, self);
    }
    return err;
}

errno_t Inode_createChannel(InodeRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    switch (Inode_GetMode(self) & S_IFMT) {
        case S_IFDIR:
            return DirectoryChannel_Create(self, pOutChannel);

        case S_IFREG:
            return FileChannel_Create(self, mode, pOutChannel);

        default:
            *pOutChannel = NULL;
            return EPERM;
    }
}

void Inode_getInfo(InodeRef _Nonnull _Locked self, struct stat* _Nonnull pi)
{
    struct timespec now;

    if (Inode_IsModified(self)) {
        FSGetCurrentTime(&now);
    }

    if (Inode_IsAccessed(self)) {
        pi->st_atim = now;
    } else {
        pi->st_atim = self->accessTime;
    }
    if (Inode_IsUpdated(self)) {
        pi->st_mtim = now;
    } else {
        pi->st_mtim = self->modificationTime;
    }
    if (Inode_IsStatusChanged(self)) {
        pi->st_ctim = now;
    } else {
        pi->st_ctim = self->statusChangeTime;
    }
    
    pi->st_size = self->size;
    pi->st_uid = self->uid;
    pi->st_gid = self->gid;
    pi->st_mode = self->mode;
    pi->st_nlink = self->linkCount;
    pi->st_fsid = Filesystem_GetId(self->filesystem);
    pi->st_ino = self->inid;
    pi->st_blksize = Filesystem_GetNodeBlockSize(self->filesystem, self);
    pi->st_blocks = (pi->st_blksize > 0) ? ((pi->st_size + (pi->st_blksize >> 1)) / pi->st_blksize) : 0;
    pi->st_dev = 0;
    pi->st_rdev = 0;
}

void Inode_setMode(InodeRef _Nonnull _Locked self, mode_t mode)
{
    self->mode = mode;
    Inode_SetModified(self, kInodeFlag_StatusChanged);
}

void Inode_setOwner(InodeRef _Nonnull _Locked self, uid_t uid, gid_t gid)
{
    if (uid != (uid_t)-1) {
        self->uid = uid;
    }
    if (gid != (gid_t)-1) {
        self->gid = gid;
    }
    Inode_SetModified(self, kInodeFlag_StatusChanged);
}

void Inode_setTimes(InodeRef _Nonnull _Locked self, const struct timespec times[_Nullable 2])
{
    const long acc_ns = (times) ? times[UTIME_ACCESS].tv_nsec : UTIME_NOW;
    const long mod_ns = (times) ? times[UTIME_MODIFICATION].tv_nsec : UTIME_NOW;

    if (acc_ns != UTIME_OMIT) {
        if (acc_ns != UTIME_NOW) {
            self->accessTime = times[UTIME_ACCESS];
        }
        else {
            FSGetCurrentTime(&self->accessTime);
        }
    }
    if (mod_ns != UTIME_OMIT) {
        if (mod_ns != UTIME_NOW) {
            self->modificationTime = times[UTIME_MODIFICATION];
        }
        else {
            FSGetCurrentTime(&self->modificationTime);
        }
    }
    Inode_SetModified(self, kInodeFlag_StatusChanged);
}

errno_t Inode_read(InodeRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EIO;
}

errno_t Inode_write(InodeRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EIO;
}

errno_t Inode_truncate(InodeRef _Nonnull _Locked self, off_t length)
{
    return EIO;
}


any_subclass_func_defs(Inode,
func_def(deinit, Inode)
func_def(createChannel, Inode)
func_def(getInfo, Inode)
func_def(setMode, Inode)
func_def(setOwner, Inode)
func_def(setTimes, Inode)
func_def(read, Inode)
func_def(write, Inode)
func_def(truncate, Inode)
);
