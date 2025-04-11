//
//  DfsDirectory.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DfsDirectory.h"
#include "DevFSPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FSUtilities.h>


errno_t DfsDirectory_Create(DevFSRef _Nonnull fs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, ino_t pnid, DfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const TimeInterval curTime = FSGetCurrentTime();
    DfsDirectoryRef self;

    try(Inode_Create(
        class(DfsDirectory),
        (FilesystemRef)fs,
        inid,
        kFileType_Directory,
        1,
        uid,
        gid,
        permissions,
        0ll,
        curTime,
        curTime,
        curTime,
        pnid,
        (InodeRef*)&self));
    try(DfsDirectory_InsertEntry(self, inid, false, &kPathComponent_Self));
    try(DfsDirectory_InsertEntry(self, pnid, false, &kPathComponent_Parent));
    
catch:
    *pOutSelf = (DfsNodeRef)self;
    return err;
}

void DfsDirectory_deinit(DfsDirectoryRef _Nonnull self)
{
    List_ForEach(&self->entries, DfsDirectoryEntry,
        FSDeallocate(pCurNode);
    )
}

bool DfsDirectory_IsEmpty(DfsDirectoryRef _Nonnull _Locked self)
{
    return List_IsEmpty(&self->entries);
}

errno_t _Nullable DfsDirectory_GetEntryForName(DfsDirectoryRef _Nonnull _Locked self, const PathComponent* _Nonnull pc, DfsDirectoryEntry* _Nullable * _Nonnull pOutEntry)
{
    *pOutEntry = NULL;

    if (pc->count == 0) {
        return ENOENT;
    }
    else if (pc->count > MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    List_ForEach(&self->entries, DfsDirectoryEntry,
        if (PathComponent_EqualsString(pc, pCurNode->name, pCurNode->nameLength)) {
            *pOutEntry = pCurNode;
            return EOK;
        }
    )

    return ENOENT;
}

errno_t DfsDirectory_GetNameOfEntryWithId(DfsDirectoryRef _Nonnull _Locked self, ino_t inid, MutablePathComponent* _Nonnull mpc)
{
    List_ForEach(&self->entries, DfsDirectoryEntry,
        if (pCurNode->inid == inid) {
            return MutablePathComponent_SetString(mpc, pCurNode->name, pCurNode->nameLength);
        }
    )

    mpc->count = 0;
    return ENOENT;
}

// Validates that adding an entry with name 'name' and file type 'type' to this
// directory is possible. This checks things like the length of the filename and
// the link count of this directory. Returns EOK if adding the entry is possible.
// The expectation is that 'self' is locked before this function is called and
// that 'self' remains locked until after the directory entry has been added to
// self.
errno_t DfsDirectory_CanAcceptEntry(DfsDirectoryRef _Nonnull _Locked self, const PathComponent* _Nonnull name, FileType type)
{
    if (name->count > MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    if (type == kFileType_Directory) {
        // Adding a subdirectory increments our link count by 1
        if (Inode_GetLinkCount(self) >= MAX_LINK_COUNT) {
            return EMLINK;
        }
    }

    return EOK;
}

// Inserts a new directory entry of the form (pName, id) into the directory node
// 'self'.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
// NOTE: expects that you called SfsDirectory_CanAcceptEntry() before calling
// this function and that it returned EOK.
errno_t DfsDirectory_InsertEntry(DfsDirectoryRef _Nonnull _Locked self, ino_t inid, bool isChildDir, const PathComponent* _Nonnull pc)
{
    decl_try_err();
    DfsDirectoryEntry* entry;

    try(FSAllocateCleared(sizeof(DfsDirectoryEntry), (void**)&entry));
    entry->inid = inid;
    entry->nameLength = pc->count;
    memcpy(entry->name, pc->name, pc->count);

    List_InsertAfterLast(&self->entries, &entry->sibling);
    Inode_IncrementFileSize(self, sizeof(DfsDirectoryEntry));


    // Increment the link count of the directory if the child node is itself a
    // directory (accounting for its '..' entry)
    if (isChildDir) {
        Inode_Link(self);
    }


    // Mark the directory as modified
    Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);

catch:
    return err;
}

errno_t DfsDirectory_RemoveEntry(DfsDirectoryRef _Nonnull _Locked self, InodeRef _Nonnull pNodeToRemove)
{
    DfsDirectoryEntry* entry = NULL;

    List_ForEach(&self->entries, DfsDirectoryEntry,
        if (pCurNode->inid == Inode_GetId(pNodeToRemove)) {
            entry = pCurNode;
            break;
        }
    )

    if (entry == NULL) {
        return ENOENT;
    }

    List_Remove(&self->entries, &entry->sibling);
    FSDeallocate(entry);
    Inode_DecrementFileSize(self, sizeof(DfsDirectoryEntry));


    // If this is a directory then unlink it from its parent since we remove a
    // '..' entry that points to the parent
    if (Inode_IsDirectory(pNodeToRemove)) {
        Inode_Unlink((InodeRef)self);
    }


    // Mark the directory as modified
    Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);
}

errno_t DfsDirectory_createChannel(DfsDirectoryRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return DirectoryChannel_Create((InodeRef)self, pOutChannel);
}

errno_t DfsDirectory_read(DfsDirectoryRef _Nonnull _Locked self, DirectoryChannelRef _Nonnull _Locked ch, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    DirectoryEntry* pOutEntry = (DirectoryEntry*)pBuffer;
    off_t offset = IOChannel_GetOffset(ch);  // in terms of #entries
    ssize_t nAllDirEntriesRead = 0;
    ssize_t nBytesRead = 0;

    DfsDirectoryEntry* curEntry = (DfsDirectoryEntry*)self->entries.first;

    // Move to the first entry that we are supposed to read
    while (offset > 0) {
        curEntry = (DfsDirectoryEntry*)curEntry->sibling.next;
        offset--;
    }

    // Read as many entries as we can fit into 'nBytesToRead'
    while (curEntry && nBytesToRead >= sizeof(DirectoryEntry)) {
        pOutEntry->inid = curEntry->inid;
        memcpy(pOutEntry->name, curEntry->name, curEntry->nameLength);
        pOutEntry->name[curEntry->nameLength] = '\0';

        nBytesRead += sizeof(DirectoryEntry);
        nBytesToRead -= sizeof(DirectoryEntry);

        nAllDirEntriesRead++;
        pOutEntry++;

        curEntry = (DfsDirectoryEntry*)curEntry->sibling.next;
    }

    if (nBytesRead > 0) {
        IOChannel_IncrementOffsetBy(ch, nAllDirEntriesRead);
    }
    *nOutBytesRead = nBytesRead;

    return err;
}


class_func_defs(DfsDirectory, DfsNode,
override_func_def(deinit, DfsDirectory, Inode)
override_func_def(createChannel, DfsDirectory, Inode)
override_func_def(read, DfsDirectory, Inode)
);
