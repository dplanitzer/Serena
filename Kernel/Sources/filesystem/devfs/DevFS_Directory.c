//
//  DevFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <security/SecurityManager.h>


// Inserts a new directory entry of the form (pName, id) into the directory node
// 'pDir'.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
errno_t DevFS_InsertDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId inid, const PathComponent* _Nonnull pName)
{
    decl_try_err();
    DfsDirectoryItem* ip = Inode_GetDfsDirectoryItem(pDir);

    err = DfsDirectoryItem_AddEntry(ip, inid, pName);
    if (err == EOK) {
        Inode_IncrementFileSize(pDir, sizeof(DfsDirectoryEntry));

        // Mark the directory as modified
        if (err == EOK) {
            Inode_SetModified(pDir, kInodeFlag_Updated | kInodeFlag_StatusChanged);
        }
    }

    return err;
}

errno_t DevFS_RemoveDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId idToRemove)
{
    const errno_t err = DfsDirectoryItem_RemoveEntry(Inode_GetDfsDirectoryItem(pDir), idToRemove);
    
    if (err == EOK) {
        Inode_DecrementFileSize(pDir, sizeof(DfsDirectoryEntry));
    }

    return err;
}

errno_t DevFS_acquireRootDirectory(DevFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    try_bang(SELock_LockShared(&self->seLock)); 
    const errno_t err = (self->flags.isMounted) ? Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirInodeId, pOutDir) : EIO;
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_acquireNodeForName(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, UserId uid, GroupId gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    DfsDirectoryEntry* entry;

    try_bang(SELock_LockShared(&self->seLock));
    err = DfsDirectoryItem_GetEntryForName(Inode_GetDfsDirectoryItem(pDir), pName, &entry);
    if (err == EOK && pOutNode) {
        err = Filesystem_AcquireNodeWithId((FilesystemRef)self, entry->inid, pOutNode);
    }
    if (err != EOK && pOutNode) {
        *pOutNode = NULL;
    }
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_getNameOfNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, UserId uid, GroupId gid, MutablePathComponent* _Nonnull pName)
{
    try_bang(SELock_LockShared(&self->seLock));
    const errno_t err = DfsDirectoryItem_GetNameOfEntryWithId(Inode_GetDfsDirectoryItem(pDir), id, pName);
    SELock_Unlock(&self->seLock);
    return err;
}


errno_t DevFS_readDirectory(DevFSRef _Nonnull self, DirectoryChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    DirectoryEntry* pOutEntry = (DirectoryEntry*)pBuffer;
    FileOffset offset = IOChannel_GetOffset(pChannel);  // in terms of #entries
    ssize_t nAllDirEntriesRead = 0;
    ssize_t nBytesRead = 0;

    try_bang(SELock_LockShared(&self->seLock));

    DfsDirectoryItem* ip = Inode_GetDfsDirectoryItem(DirectoryChannel_GetInode(pChannel));
    DfsDirectoryEntry* curEntry = (DfsDirectoryEntry*)ip->entries.first;

    // Move to the first entry that we are supposed to read
    while (offset > 0) {
        curEntry = (DfsDirectoryEntry*)curEntry->sibling.next;
        offset--;
    }

    // Read as many entries as we can fit into 'nBytesToRead'
    while (curEntry && nBytesToRead >= sizeof(DirectoryEntry)) {
        pOutEntry->inodeId = curEntry->inid;
        memcpy(pOutEntry->name, curEntry->name, curEntry->nameLength);
        pOutEntry->name[curEntry->nameLength] = '\0';

        nBytesRead += sizeof(DirectoryEntry);
        nBytesToRead -= sizeof(DirectoryEntry);

        nAllDirEntriesRead++;
        pOutEntry++;

        curEntry = (DfsDirectoryEntry*)curEntry->sibling.next;
    }

    if (nBytesRead > 0) {
        IOChannel_IncrementOffsetBy(pChannel, nAllDirEntriesRead);
    }
    *nOutBytesRead = nBytesRead;

    SELock_Unlock(&self->seLock);

    return err;
}
