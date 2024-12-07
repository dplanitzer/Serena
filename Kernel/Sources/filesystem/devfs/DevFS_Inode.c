//
//  DevFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"


errno_t DevFS_createNode(DevFSRef _Nonnull self, FileType type, User user, FilePermissions permissions, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, void* _Nullable dirInsertionHint, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const TimeInterval curTime = FSGetCurrentTime();
    DfsItem* pItem = NULL;
    InodeRef pNode = NULL;

    try_bang(SELock_LockExclusive(&self->seLock));

    // We must have write permissions for the parent directory
    try(Filesystem_CheckAccess(self, dir, user, kAccess_Writable));


    switch (type) {
        case kFileType_Directory:
            // Make sure that the parent directory is able to accept one more link
            if (Inode_GetLinkCount(dir) >= MAX_LINK_COUNT) {
                throw(EMLINK);
            }

            try(DfsDirectoryItem_Create(DevFS_GetNextAvailableInodeId(self), permissions, user.uid, user.gid, Inode_GetId(dir), (DfsDirectoryItem**)&pItem));
            break;

        case kFileType_Device:
            abort();
            // XXX fix me I guess or so or not (publish)
            //try(DfsDriverItem_Create(DevFS_GetNextAvailableInodeId(self), permissions, user.uid, user.gid, pDriver, (DfsDriverItem**)&pItem));
            break;

        default:
            throw(EIO);
            break;
    }

    DevFS_AddItem(self, pItem);

    try(DevFS_InsertDirectoryEntry(self, dir, pItem->inid, name));
    if (type == kFileType_Directory) {
        // Increment the parent directory link count to account for the '..' entry
        // in the just created subdirectory
        Inode_Link(dir);
    }

    if ((err = Filesystem_AcquireNodeWithId((FilesystemRef)self, pItem->inid, &pNode)) == EOK) {
        Inode_SetModified(pNode, kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }

    *pOutNode = pNode;
    SELock_Unlock(&self->seLock);

    return err;

catch:
    if (pItem) {
        DevFS_RemoveItem(self, pItem->inid);
        DfsItem_Destroy(pItem);
    }

    *pOutNode = NULL;
    SELock_Unlock(&self->seLock);

    return err;
}

errno_t DevFS_onReadNodeFromDisk(DevFSRef _Nonnull self, InodeId inid, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
//print("onReadNodeFromDisk(%d)", inid);
    DfsItem* ip = DevFS_GetItem(self, inid);
    if (ip == NULL) {
//        print(" -> failed\n");
        *pOutNode = NULL;
        return EIO;
    }
//print(" -> ok\n");
    return Inode_Create(
        (FilesystemRef)self,
        inid,
        ip->type,
        ip->linkCount,
        ip->uid,
        ip->gid,
        ip->permissions,
        ip->size,
        ip->accessTime,
        ip->modificationTime,
        ip->statusChangeTime,
        ip,
        pOutNode);
}

errno_t DevFS_onWriteNodeToDisk(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    const TimeInterval curTime = FSGetCurrentTime();
    DfsItem* ip = Inode_GetDfsItem(pNode);

    ip->accessTime = (Inode_IsAccessed(pNode)) ? curTime : Inode_GetAccessTime(pNode);
    ip->modificationTime = (Inode_IsUpdated(pNode)) ? curTime : Inode_GetModificationTime(pNode);
    ip->statusChangeTime = (Inode_IsStatusChanged(pNode)) ? curTime : Inode_GetStatusChangeTime(pNode);
    ip->size = Inode_GetFileSize(pNode);
    ip->uid = Inode_GetUserId(pNode);
    ip->gid = Inode_GetGroupId(pNode);
    ip->linkCount = Inode_GetLinkCount(pNode);
    ip->permissions = Inode_GetFilePermissions(pNode);
    ip->type = Inode_GetFileType(pNode);

    return EOK;
}

void DevFS_onRemoveNodeFromDisk(DevFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    DevFS_RemoveItem(self, Inode_GetId(pNode));
    DfsItem_Destroy(Inode_GetDfsItem(pNode));
}
