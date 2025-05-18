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
    FileType type, int linkCount, uid_t uid, gid_t gid, FilePermissions permissions,
    off_t size, struct timespec accessTime, struct timespec modTime, struct timespec statusChangeTime,
    ino_t pnid,
    InodeRef _Nullable * _Nonnull pOutNode)
{
    InodeRef self;

    const errno_t err = FSAllocateCleared(pClass->instanceSize, (void**) &self);
    if (err == EOK) {
        self->super.clazz = pClass;
        self->useCount = 0;
        self->state = kInodeState_Reading;

        Lock_Init(&self->lock);
        self->accessTime = accessTime;
        self->modificationTime = modTime;
        self->statusChangeTime = statusChangeTime;
        self->size = size;
        self->filesystem = pFS;
        self->inid = id;
        self->pnid = pnid;
        self->linkCount = linkCount;
        self->type = type;
        self->flags = 0;
        self->permissions = permissions;
        self->uid = uid;
        self->gid = gid;
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
        Lock_Deinit(&self->lock);

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
    switch (Inode_GetFileType(self)) {
        case S_IFDIR:
            return DirectoryChannel_Create(self, pOutChannel);

        case S_IFREG:
            return FileChannel_Create(self, mode, pOutChannel);

        default:
            *pOutChannel = NULL;
            return EPERM;
    }
}

errno_t Inode_getInfo(InodeRef _Nonnull _Locked self, finfo_t* _Nonnull pOutInfo)
{
    struct timespec curTime;

    if (Inode_IsModified(self)) {
        curTime = FSGetCurrentTime();
    }

    if (Inode_IsAccessed(self)) {
        pOutInfo->accessTime = curTime;
    } else {
        pOutInfo->accessTime = Inode_GetAccessTime(self);
    }
    if (Inode_IsUpdated(self)) {
        pOutInfo->modificationTime = curTime;
    } else {
        pOutInfo->modificationTime = Inode_GetModificationTime(self);
    }
    if (Inode_IsStatusChanged(self)) {
        pOutInfo->statusChangeTime = curTime;
    } else {
        pOutInfo->statusChangeTime = Inode_GetStatusChangeTime(self);
    }
    
    pOutInfo->size = Inode_GetFileSize(self);
    pOutInfo->uid = Inode_GetUserId(self);
    pOutInfo->gid = Inode_GetGroupId(self);
    pOutInfo->permissions = Inode_GetFilePermissions(self);
    pOutInfo->type = Inode_GetFileType(self);
    pOutInfo->reserved = 0;
    pOutInfo->linkCount = Inode_GetLinkCount(self);
    pOutInfo->fsid = Filesystem_GetId(Inode_GetFilesystem(self));
    pOutInfo->inid = Inode_GetId(self);

    return EOK;
}

errno_t Inode_setInfo(InodeRef _Nonnull _Locked self, uid_t uid, gid_t gid, fmutinfo_t* _Nonnull pInfo)
{
    const uint32_t  modify = pInfo->modify & kModifyFileInfo_All;

    if (modify == 0) {
        return EOK;
    }

    // Only the owner of a file may change its metadata.
    if (uid != Inode_GetUserId(self) && !SecurityManager_IsSuperuser(gSecurityManager, uid)) {
        return EPERM;
    }

    // Can't change the metadata if the disk is read-only
    if (Filesystem_IsReadOnly(Inode_GetFilesystem(self))) {
        return EROFS;
    }
    

    // We got permissions. Now update the data as requested.
    if ((modify & kModifyFileInfo_UserId) == kModifyFileInfo_UserId) {
        Inode_SetUserId(self, pInfo->uid);
    }

    if ((modify & kModifyFileInfo_GroupId) == kModifyFileInfo_GroupId) {
        Inode_SetGroupId(self, pInfo->gid);
    }

    if ((modify & kModifyFileInfo_Permissions) == kModifyFileInfo_Permissions) {
        const FilePermissions oldPerms = Inode_GetFilePermissions(self);
        const FilePermissions newPerms = (oldPerms & ~pInfo->permissionsModifyMask) | (pInfo->permissions & pInfo->permissionsModifyMask);

        Inode_SetFilePermissions(self, newPerms);
    }

    // Update timestamps
    if ((modify & kModifyFileInfo_AccessTime) == kModifyFileInfo_AccessTime) {
        Inode_SetAccessTime(self, pInfo->accessTime);
    }
    if ((modify & kModifyFileInfo_ModificationTime) == kModifyFileInfo_ModificationTime) {
        Inode_SetModificationTime(self, pInfo->modificationTime);
    }

    Inode_SetModified(self, kInodeFlag_StatusChanged);

    return EOK;
}

errno_t Inode_setMode(InodeRef _Nonnull _Locked self, mode_t mode)
{
    self->permissions = mode;
    Inode_SetModified(self, kInodeFlag_StatusChanged);

    return EOK;
}

errno_t Inode_setOwner(InodeRef _Nonnull _Locked self, uid_t uid, gid_t gid)
{
    if (uid != (uid_t)-1) {
        self->uid = uid;
    }
    if (gid != (gid_t)-1) {
        self->gid = gid;
    }
    Inode_SetModified(self, kInodeFlag_StatusChanged);

    return EOK;
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
func_def(setInfo, Inode)
func_def(setMode, Inode)
func_def(setOwner, Inode)
func_def(read, Inode)
func_def(write, Inode)
func_def(truncate, Inode)
);
