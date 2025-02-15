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
#include <security/SecurityManager.h>


// Returns the next available FSID.
static FilesystemId Filesystem_GetNextAvailableId(void)
{
    // XXX want to:
    // XXX handle overflow (wrap around)
    // XXX make sure the generated id isn't actually in use by someone else
    static volatile AtomicInt gNextAvailableId = 0;
    return (FilesystemId) AtomicInt_Increment(&gNextAvailableId);
}

errno_t Filesystem_Create(Class* pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    FilesystemRef self;

    try(_Object_Create(pClass, 0, (ObjectRef*)&self));
    self->fsid = Filesystem_GetNextAvailableId();
    Lock_Init(&self->inodeManagementLock);
    PointerArray_Init(&self->inodesInUse, 16);

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void Filesystem_deinit(FilesystemRef _Nonnull self)
{
    PointerArray_Deinit(&self->inodesInUse);
    Lock_Deinit(&self->inodeManagementLock);
}

errno_t Filesystem_PublishNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    decl_try_err();

    Lock_Lock(&self->inodeManagementLock);
    assert(pNode->useCount == 0);
    try(PointerArray_Add(&self->inodesInUse, pNode));
    pNode->useCount++;
    
catch:
    Lock_Unlock(&self->inodeManagementLock);
    return err;
}

errno_t Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef pNode = NULL;

    Lock_Lock(&self->inodeManagementLock);

    for (int i = 0; i < PointerArray_GetCount(&self->inodesInUse); i++) {
        InodeRef pCurNode = (InodeRef)PointerArray_GetAt(&self->inodesInUse, i);

        if (Inode_GetId(pCurNode) == id) {
            pNode = pCurNode;
            break;
        }
    }

    if (pNode == NULL) {
        try(Filesystem_OnReadNodeFromDisk(self, id, &pNode));
        try(PointerArray_Add(&self->inodesInUse, pNode));
    }

    pNode->useCount++;
    Lock_Unlock(&self->inodeManagementLock);
    *pOutNode = pNode;

    return EOK;

catch:
    Lock_Unlock(&self->inodeManagementLock);
    *pOutNode = NULL;
    return err;
}

InodeRef _Nonnull _Locked Filesystem_ReacquireNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    Lock_Lock(&self->inodeManagementLock);
    pNode->useCount++;
    Lock_Unlock(&self->inodeManagementLock);

    return pNode;
}

errno_t Filesystem_RelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nullable pNode)
{
    decl_try_err();

    if (pNode == NULL) {
        return EOK;
    }
    
    Lock_Lock(&self->inodeManagementLock);

    if (!Filesystem_IsReadOnly(self)) {
        // Remove the inode (file) from disk if the use count goes to 0 and the link
        // count is 0. The reason why we defer deleting the on-disk version of the
        // inode is that this allows you to create a file, immediately unlink it and
        // that you are then still able to read/write the file until you close it.
        // This gives you anonymous temporary storage that is guaranteed to disappear
        // with the death of the process.  
        if (pNode->useCount == 1 && pNode->linkCount == 0) {
            Filesystem_OnRemoveNodeFromDisk(self, pNode);
        }
        else if (Inode_IsModified(pNode)) {
            err = Filesystem_OnWriteNodeToDisk(self, pNode);
            Inode_ClearModified(pNode);
        }
    }

    assert(pNode->useCount > 0);
    pNode->useCount--;
    if (pNode->useCount == 0) {
        PointerArray_Remove(&self->inodesInUse, pNode);
        Inode_Destroy(pNode);
    }

    Lock_Unlock(&self->inodeManagementLock);

    return err;
}

bool Filesystem_CanUnmount(FilesystemRef _Nonnull self)
{
    Lock_Lock(&self->inodeManagementLock);
    const bool ok = PointerArray_IsEmpty(&self->inodesInUse);
    Lock_Unlock(&self->inodeManagementLock);
    return ok;
}

errno_t Filesystem_onReadNodeFromDisk(FilesystemRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode)
{
    return EIO;
}

errno_t Filesystem_onWriteNodeToDisk(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    return EIO;
}

void Filesystem_onRemoveNodeFromDisk(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
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

errno_t Filesystem_acquireNodeForName(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, UserId uid, GroupId gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
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

errno_t Filesystem_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, UserId uid, GroupId gid, MutablePathComponent* _Nonnull pName)
{
    pName->count = 0;
    return EIO;
}

errno_t Filesystem_createChannel(FilesystemRef _Nonnull self, InodeRef _Consuming _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    switch (Inode_GetFileType(pNode)) {
        case kFileType_Directory:
            return DirectoryChannel_Create(pNode, pOutChannel);

        case kFileType_RegularFile:
            return FileChannel_Create(pNode, mode, pOutChannel);

        default:
            *pOutChannel = NULL;
            return EPERM;
    }
}

errno_t Filesystem_getFileInfo(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo)
{
    TimeInterval curTime;

    if (Inode_IsModified(pNode)) {
        curTime = FSGetCurrentTime();
    }

    if (Inode_IsAccessed(pNode)) {
        pOutInfo->accessTime = curTime;
    } else {
        pOutInfo->accessTime = Inode_GetAccessTime(pNode);
    }
    if (Inode_IsUpdated(pNode)) {
        pOutInfo->modificationTime = curTime;
    } else {
        pOutInfo->modificationTime = Inode_GetModificationTime(pNode);
    }
    if (Inode_IsStatusChanged(pNode)) {
        pOutInfo->statusChangeTime = curTime;
    } else {
        pOutInfo->statusChangeTime = Inode_GetStatusChangeTime(pNode);
    }
    
    pOutInfo->size = Inode_GetFileSize(pNode);
    pOutInfo->uid = Inode_GetUserId(pNode);
    pOutInfo->gid = Inode_GetGroupId(pNode);
    pOutInfo->permissions = Inode_GetFilePermissions(pNode);
    pOutInfo->type = Inode_GetFileType(pNode);
    pOutInfo->reserved = 0;
    pOutInfo->linkCount = Inode_GetLinkCount(pNode);
    pOutInfo->filesystemId = Filesystem_GetId(self);
    pOutInfo->inodeId = Inode_GetId(pNode);

    return EOK;
}

errno_t Filesystem_setFileInfo(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, UserId uid, GroupId gid, MutableFileInfo* _Nonnull pInfo)
{
    const uint32_t  modify = pInfo->modify & kModifyFileInfo_All;

    if (modify == 0) {
        return EOK;
    }

    // Only the owner of a file may change its metadata.
    if (uid != Inode_GetUserId(pNode) && !SecurityManager_IsSuperuser(gSecurityManager, uid)) {
        return EPERM;
    }

    // Can't change the metadata if the disk is read-only
    if (Filesystem_IsReadOnly(self)) {
        return EROFS;
    }
    

    // We got permissions. Now update the data as requested.
    if ((modify & kModifyFileInfo_UserId) == kModifyFileInfo_UserId) {
        Inode_SetUserId(pNode, pInfo->uid);
    }

    if ((modify & kModifyFileInfo_GroupId) == kModifyFileInfo_GroupId) {
        Inode_SetGroupId(pNode, pInfo->gid);
    }

    if ((modify & kModifyFileInfo_Permissions) == kModifyFileInfo_Permissions) {
        const FilePermissions oldPerms = Inode_GetFilePermissions(pNode);
        const FilePermissions newPerms = (oldPerms & ~pInfo->permissionsModifyMask) | (pInfo->permissions & pInfo->permissionsModifyMask);

        Inode_SetFilePermissions(pNode, newPerms);
    }

    // Update timestamps
    if ((modify & kModifyFileInfo_AccessTime) == kModifyFileInfo_AccessTime) {
        Inode_SetAccessTime(pNode, pInfo->accessTime);
    }
    if ((modify & kModifyFileInfo_ModificationTime) == kModifyFileInfo_ModificationTime) {
        Inode_SetModificationTime(pNode, pInfo->modificationTime);
    }

    Inode_SetModified(pNode, kInodeFlag_StatusChanged);

    return EOK;
}

errno_t Filesystem_readFile(FilesystemRef _Nonnull self, FileChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EIO;
}

errno_t Filesystem_writeFile(FilesystemRef _Nonnull self, FileChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EIO;
}

errno_t Filesystem_truncateFile(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pFile, FileOffset length)
{
    return EIO;
}

errno_t Filesystem_readDirectory(FilesystemRef _Nonnull self, DirectoryChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EIO;
}

errno_t Filesystem_createNode(FilesystemRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsertionHint, UserId uid, GroupId gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
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

errno_t Filesystem_move(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, UserId uid, GroupId gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    return EACCESS;
}

errno_t Filesystem_rename(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, UserId uid, GroupId gid)
{
    return EACCESS;
}


class_func_defs(Filesystem, Object,
override_func_def(deinit, Filesystem, Object)
func_def(onReadNodeFromDisk, Filesystem)
func_def(onWriteNodeToDisk, Filesystem)
func_def(onRemoveNodeFromDisk, Filesystem)
func_def(start, Filesystem)
func_def(stop, Filesystem)
func_def(acquireRootDirectory, Filesystem)
func_def(acquireNodeForName, Filesystem)
func_def(getNameOfNode, Filesystem)
func_def(createChannel, Filesystem)
func_def(getFileInfo, Filesystem)
func_def(setFileInfo, Filesystem)
func_def(createNode, Filesystem)
func_def(readFile, Filesystem)
func_def(writeFile, Filesystem)
func_def(truncateFile, Filesystem)
func_def(readDirectory, Filesystem)
func_def(isReadOnly, Filesystem)
func_def(unlink, Filesystem)
func_def(move, Filesystem)
func_def(rename, Filesystem)
);
