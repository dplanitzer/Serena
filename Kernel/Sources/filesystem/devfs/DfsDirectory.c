//
//  DfsDirectory.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DfsDirectory.h"
#include "DevFSPriv.h"
#include <driver/Driver.h>
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>


errno_t DfsDirectory_Create(DevFSRef _Nonnull fs, InodeId inid, DfsDirectoryItem* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DfsDirectoryRef self;

    err = Inode_Create(
        class(DfsDirectory),
        (FilesystemRef)fs,
        inid,
        kFileType_Directory,
        ip->super.linkCount,
        ip->super.uid,
        ip->super.gid,
        ip->super.permissions,
        ip->super.size,
        ip->super.accessTime,
        ip->super.modificationTime,
        ip->super.statusChangeTime,
        (InodeRef*)&self);
    if (err == EOK) {
        self->item = ip;
    }
    *pOutNode = (InodeRef)self;
    return err;
}

void DfsDirectory_Serialize(InodeRef _Nonnull _Locked self)
{
    DfsDirectoryItem* _Nonnull ip = DfsDirectory_GetItem(self);
    const TimeInterval curTime = FSGetCurrentTime();

    ip->super.accessTime = (Inode_IsAccessed(self)) ? curTime : Inode_GetAccessTime(self);
    ip->super.modificationTime = (Inode_IsUpdated(self)) ? curTime : Inode_GetModificationTime(self);
    ip->super.statusChangeTime = (Inode_IsStatusChanged(self)) ? curTime : Inode_GetStatusChangeTime(self);
    ip->super.size = Inode_GetFileSize(self);
    ip->super.uid = Inode_GetUserId(self);
    ip->super.gid = Inode_GetGroupId(self);
    ip->super.linkCount = Inode_GetLinkCount(self);
    ip->super.permissions = Inode_GetFilePermissions(self);
}

errno_t DfsDirectory_createChannel(InodeRef _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return DirectoryChannel_Create(self, pOutChannel);
}

errno_t DfsDirectory_read(InodeRef _Nonnull _Locked self, DirectoryChannelRef _Nonnull _Locked ch, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    DevFSRef fs = Inode_GetFilesystemAs(self, DevFS);
    DirectoryEntry* pOutEntry = (DirectoryEntry*)pBuffer;
    FileOffset offset = IOChannel_GetOffset(ch);  // in terms of #entries
    ssize_t nAllDirEntriesRead = 0;
    ssize_t nBytesRead = 0;

    try_bang(SELock_LockShared(&fs->seLock));

    DfsDirectoryItem* ip = DfsDirectory_GetItem(self);
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
        IOChannel_IncrementOffsetBy(ch, nAllDirEntriesRead);
    }
    *nOutBytesRead = nBytesRead;

    SELock_Unlock(&fs->seLock);

    return err;
}


class_func_defs(DfsDirectory, Inode,
override_func_def(createChannel, DfsDirectory, Inode)
override_func_def(read, DfsDirectory, Inode)
);
