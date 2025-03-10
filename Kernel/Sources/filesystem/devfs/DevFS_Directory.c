//
//  DevFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"
#include "DfsDirectory.h"
#include <filesystem/DirectoryChannel.h>


errno_t DevFS_acquireRootDirectory(DevFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    try_bang(SELock_LockShared(&self->seLock)); 
    const errno_t err = (self->flags.isMounted) ? Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirInodeId, pOutDir) : EIO;
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t DevFS_acquireNodeForName(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    DfsDirectoryEntry* entry;

    try_bang(SELock_LockShared(&self->seLock));
    err = DfsDirectory_GetEntryForName((DfsDirectoryRef)pDir, pName, &entry);
    if (err == EOK && pOutNode) {
        err = Filesystem_AcquireNodeWithId((FilesystemRef)self, entry->inid, pOutNode);
    }
    if (err != EOK && pOutNode) {
        *pOutNode = NULL;
    }
    SELock_Unlock(&self->seLock);

    return err;
}

errno_t DevFS_getNameOfNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, uid_t uid, gid_t gid, MutablePathComponent* _Nonnull pName)
{
    try_bang(SELock_LockShared(&self->seLock));
    const errno_t err = DfsDirectory_GetNameOfEntryWithId((DfsDirectoryRef)pDir, id, pName);
    SELock_Unlock(&self->seLock);
    return err;
}
