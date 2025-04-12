//
//  DevFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"
#include "DfsDirectory.h"


errno_t DevFS_acquireNodeForName(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    DfsDirectoryEntry* entry;

    err = DfsDirectory_GetEntryForName((DfsDirectoryRef)pDir, pName, &entry);
    if (err == EOK && pOutNode) {
        err = Filesystem_AcquireNodeWithId((FilesystemRef)self, entry->inid, pOutNode);
    }
    if (err != EOK && pOutNode) {
        *pOutNode = NULL;
    }

    return err;
}

errno_t DevFS_getNameOfNode(DevFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, MutablePathComponent* _Nonnull pName)
{
    return DfsDirectory_GetNameOfEntryWithId((DfsDirectoryRef)pDir, id, pName);
}
