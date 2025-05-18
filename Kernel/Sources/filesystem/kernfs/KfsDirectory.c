//
//  KfsDirectory.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "KfsDirectory.h"
#include "KernFSPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FSUtilities.h>
#include <kern/string.h>


errno_t KfsDirectory_Create(KernFSRef _Nonnull fs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, ino_t pnid, KfsNodeRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const struct timespec curTime = FSGetCurrentTime();
    KfsDirectoryRef self;

    try(Inode_Create(
        class(KfsDirectory),
        (FilesystemRef)fs,
        inid,
        S_IFDIR,
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
    try(KfsDirectory_InsertEntry(self, inid, false, &kPathComponent_Self));
    try(KfsDirectory_InsertEntry(self, pnid, false, &kPathComponent_Parent));
    
catch:
    *pOutSelf = (KfsNodeRef)self;
    return err;
}

void KfsDirectory_deinit(KfsDirectoryRef _Nonnull self)
{
    List_ForEach(&self->entries, KfsDirectoryEntry,
        FSDeallocate(pCurNode);
    )
}

bool KfsDirectory_IsEmpty(KfsDirectoryRef _Nonnull _Locked self)
{
    return List_IsEmpty(&self->entries);
}

errno_t _Nullable KfsDirectory_GetEntryForName(KfsDirectoryRef _Nonnull _Locked self, const PathComponent* _Nonnull pc, KfsDirectoryEntry* _Nullable * _Nonnull pOutEntry)
{
    *pOutEntry = NULL;

    if (pc->count == 0) {
        return ENOENT;
    }
    else if (pc->count > MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    List_ForEach(&self->entries, KfsDirectoryEntry,
        if (PathComponent_EqualsString(pc, pCurNode->name, pCurNode->nameLength)) {
            *pOutEntry = pCurNode;
            return EOK;
        }
    )

    return ENOENT;
}

errno_t KfsDirectory_GetNameOfEntryWithId(KfsDirectoryRef _Nonnull _Locked self, ino_t inid, MutablePathComponent* _Nonnull mpc)
{
    List_ForEach(&self->entries, KfsDirectoryEntry,
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
errno_t KfsDirectory_CanAcceptEntry(KfsDirectoryRef _Nonnull _Locked self, const PathComponent* _Nonnull name, FileType type)
{
    if (name->count > MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    if (type == S_IFDIR) {
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
errno_t KfsDirectory_InsertEntry(KfsDirectoryRef _Nonnull _Locked self, ino_t inid, bool isChildDir, const PathComponent* _Nonnull pc)
{
    decl_try_err();
    KfsDirectoryEntry* entry;

    try(FSAllocateCleared(sizeof(KfsDirectoryEntry), (void**)&entry));
    entry->inid = inid;
    entry->nameLength = pc->count;
    memcpy(entry->name, pc->name, pc->count);

    List_InsertAfterLast(&self->entries, &entry->sibling);
    Inode_IncrementFileSize(self, sizeof(KfsDirectoryEntry));


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

errno_t KfsDirectory_RemoveEntry(KfsDirectoryRef _Nonnull _Locked self, InodeRef _Nonnull pNodeToRemove)
{
    KfsDirectoryEntry* entry = NULL;

    List_ForEach(&self->entries, KfsDirectoryEntry,
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
    Inode_DecrementFileSize(self, sizeof(KfsDirectoryEntry));


    // If this is a directory then unlink it from its parent since we remove a
    // '..' entry that points to the parent
    if (Inode_IsDirectory(pNodeToRemove)) {
        Inode_Unlink((InodeRef)self);
    }


    // Mark the directory as modified
    Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);
}

errno_t KfsDirectory_createChannel(KfsDirectoryRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return DirectoryChannel_Create((InodeRef)self, pOutChannel);
}

errno_t KfsDirectory_read(KfsDirectoryRef _Nonnull _Locked self, DirectoryChannelRef _Nonnull _Locked ch, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    struct dirent* pOutEntry = (struct dirent*)pBuffer;
    off_t offset = IOChannel_GetOffset(ch);  // in terms of #entries
    ssize_t nAllDirEntriesRead = 0;
    ssize_t nBytesRead = 0;

    KfsDirectoryEntry* curEntry = (KfsDirectoryEntry*)self->entries.first;

    // Move to the first entry that we are supposed to read
    while (offset > 0) {
        curEntry = (KfsDirectoryEntry*)curEntry->sibling.next;
        offset--;
    }

    // Read as many entries as we can fit into 'nBytesToRead'
    while (curEntry && nBytesToRead >= sizeof(struct dirent)) {
        pOutEntry->inid = curEntry->inid;
        memcpy(pOutEntry->name, curEntry->name, curEntry->nameLength);
        pOutEntry->name[curEntry->nameLength] = '\0';

        nBytesRead += sizeof(struct dirent);
        nBytesToRead -= sizeof(struct dirent);

        nAllDirEntriesRead++;
        pOutEntry++;

        curEntry = (KfsDirectoryEntry*)curEntry->sibling.next;
    }

    if (nBytesRead > 0) {
        IOChannel_IncrementOffsetBy(ch, nAllDirEntriesRead);
    }
    *nOutBytesRead = nBytesRead;

    return err;
}


class_func_defs(KfsDirectory, KfsNode,
override_func_def(deinit, KfsDirectory, Inode)
override_func_def(createChannel, KfsDirectory, Inode)
override_func_def(read, KfsDirectory, Inode)
);
