//
//  DevFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"



// Inserts a new directory entry of the form (pName, id) into the directory node
// 'pDir'.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
errno_t DevFS_InsertDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId inid, const PathComponent* _Nonnull pName)
{
    decl_try_err();
    DfsDirectoryItem* ip = Inode_GetDfsDirectoryItem(pDir);

    try(DfsDirectoryItem_AddEntry(ip, inid, pName));
    Inode_IncrementFileSize(pDir, sizeof(DfsDirectoryEntry));


    // Mark the directory as modified
    if (err == EOK) {
        Inode_SetModified(pDir, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }

catch:
    return err;
}

errno_t DevFS_RemoveDirectoryEntry(DevFSRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeId idToRemove)
{
    decl_try_err();

    try(DfsDirectoryItem_RemoveEntry(Inode_GetDfsDirectoryItem(pDir), idToRemove));
    Inode_DecrementFileSize(pDir, sizeof(DfsDirectoryEntry));

catch:
    return err;
}

errno_t DevFS_acquireRootDirectory(DevFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    try_bang(SELock_LockShared(&self->seLock)); 
    err = (self->flags.isMounted) ? Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirInodeId, pOutDir) : EIO;
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_acquireNodeForName(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, User user, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    DfsDirectoryEntry* entry;

    try_bang(SELock_LockShared(&self->seLock));
    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Searchable));
    try(DfsDirectoryItem_GetEntryForName(Inode_GetDfsDirectoryItem(pDir), pName, &entry));

    if (pOutNode) {
        try(Filesystem_AcquireNodeWithId((FilesystemRef)self, entry->inid, pOutNode));
    }
    SELock_Unlock(&self->seLock);
    return EOK;

catch:
    SELock_Unlock(&self->seLock);
    if (pOutNode) {
        *pOutNode = NULL;
    }
    return err;
}

errno_t DevFS_getNameOfNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, User user, MutablePathComponent* _Nonnull pName)
{
    decl_try_err();
    DfsDirectoryEntry* entry;

    try_bang(SELock_LockShared(&self->seLock));
    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Readable | kAccess_Searchable));
    try(DfsDirectoryItem_GetEntryForId(Inode_GetDfsDirectoryItem(pDir), id, &entry));

    const ssize_t len = String_LengthUpTo(entry->name, MAX_NAME_LENGTH);
    if (len > pName->capacity) {
        throw(ERANGE);
    }

    String_CopyUpTo(pName->name, entry->name, len);
    pName->count = len;

    SELock_Unlock(&self->seLock);
    return EOK;

catch:
    SELock_Unlock(&self->seLock);
    pName->count = 0;
    return err;
}


errno_t DevFS_openDirectory(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, User user)
{
    return Filesystem_CheckAccess(self, pDir, user, kAccess_Readable);
}

errno_t DevFS_readDirectory(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    DirectoryEntry* pOutEntry = (DirectoryEntry*)pBuffer;
    FileOffset offset = *pInOutOffset;  // in terms of #entries
    ssize_t nAllDirEntriesRead = 0;
    ssize_t nBytesRead = 0;

    try_bang(SELock_LockShared(&self->seLock));

    DfsDirectoryItem* ip = Inode_GetDfsDirectoryItem(pDir);
    DfsDirectoryEntry* curEntry = (DfsDirectoryEntry*)ip->entries.first;

    // Move to the first entry that we are supposed to read
    while (offset > 0) {
        curEntry = (DfsDirectoryEntry*)curEntry->sibling.next;
        offset--;
    }

    // Read as many entries as we can fit into 'nBytesToRead'
    while (curEntry && nBytesToRead >= sizeof(DirectoryEntry)) {
        pOutEntry->inodeId = curEntry->inid;
        String_CopyUpTo(pOutEntry->name, curEntry->name, MAX_NAME_LENGTH);

        nBytesRead += sizeof(DirectoryEntry);
        nBytesToRead -= sizeof(DirectoryEntry);

        nAllDirEntriesRead++;
        pOutEntry++;

        curEntry = (DfsDirectoryEntry*)curEntry->sibling.next;
    }

    if (nBytesRead > 0) {
        *pInOutOffset += nAllDirEntriesRead;
    }
    *nOutBytesRead = nBytesRead;

    SELock_Unlock(&self->seLock);

    return err;
}