//
//  KernFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "KernFSPriv.h"
#include "KfsDirectory.h"


errno_t KernFS_acquireNodeForName(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    KfsDirectoryEntry* entry;

    err = KfsDirectory_GetEntryForName((KfsDirectoryRef)pDir, pName, &entry);
    if (err == EOK && pOutNode) {
        err = Filesystem_AcquireNodeWithId((FilesystemRef)self, entry->inid, pOutNode);
    }
    if (err != EOK && pOutNode) {
        *pOutNode = NULL;
    }

    return err;
}

errno_t KernFS_getNameOfNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, MutablePathComponent* _Nonnull pName)
{
    return KfsDirectory_GetNameOfEntryWithId((KfsDirectoryRef)pDir, id, pName);
}
