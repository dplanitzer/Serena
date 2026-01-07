//
//  FSManager.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "FSManager.h"
#include <ext/perm.h>
#include <filemanager/FileHierarchy.h>
#include <filesystem/FSUtilities.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <assert.h>
#include <stdlib.h>
#include <kpi/stat.h>


errno_t FSManager_Create(RamContainerRef _Nonnull fsContainer, FSManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FSManagerRef self = NULL;
    InodeRef rootDir = NULL;
    FileHierarchyRef fh = NULL;

    try_null(self, malloc(sizeof(FSManager)), ENOMEM);

    try(SerenaFS_Create((FSContainerRef)fsContainer, (SerenaFSRef*)&self->fs));
    try(Filesystem_Start(self->fs, ""));

    try(FileHierarchy_Create(self->fs, &fh));
    rootDir = FileHierarchy_AcquireRootDirectory(fh);
    FileManager_Init(&self->fm, fh, kUserId_Root, kGroupId_Root, rootDir, rootDir, perm_from_octal(0));
    self->isFmUp = true;

    Inode_Relinquish(rootDir);
    Object_Release(fh);

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(fh);
    Inode_Relinquish(rootDir);
    FSManager_Destroy(self);
    *pOutSelf = NULL;

    return err;
}

void FSManager_Destroy(FSManagerRef _Nullable self)
{
    if (self) {
        if (self->isFmUp) {
            FileManager_Deinit(&self->fm);
        }

        if (self->fs) {
            assert(Filesystem_Stop(self->fs, true) == EOK);
            Filesystem_Disconnect(self->fs);
            Object_Release(self->fs);
            self->fs = NULL;
        }

        free(self);
    }
}
