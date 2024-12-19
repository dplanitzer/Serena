//
//  DiskController.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskController.h"
#include "RamFSContainer.h"
#include <filemanager/FileHierarchy.h>
#include <filesystem/Filesystem.h>
#include <filesystem/FSUtilities.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <stdlib.h>


errno_t DiskController_CreateWithContentsOfPath(const char* _Nonnull path, DiskControllerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskControllerRef self;
    FilesystemRef fs = NULL;
    InodeRef rootDir = NULL;
    FileHierarchyRef fh = NULL;

    try_null(self, malloc(sizeof(DiskController)), ENOMEM);

    try(RamFSContainer_CreateWithContentsOfPath(path, &self->fsContainer));
    try(SerenaFS_Create((FSContainerRef)self->fsContainer, (SerenaFSRef*)&fs));
    try(Filesystem_Start(fs, NULL, 0));

    try(FileHierarchy_Create(fs, &fh));
    rootDir = FileHierarchy_AcquireRootDirectory(fh);
    FileManager_Init(&self->fm, fh, kUser_Root, rootDir, rootDir, FilePermissions_MakeFromOctal(0));
    Inode_Relinquish(rootDir);
    Object_Release(fh);

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(fh);
    Inode_Relinquish(rootDir);
    DiskController_Destroy(self);
    *pOutSelf = NULL;

    return err;
}

void DiskController_Destroy(DiskControllerRef _Nullable self)
{
    if (self) {
        FileManager_Deinit(&self->fm);

        Object_Release(self->fsContainer);
        self->fsContainer = NULL;

        free(self);
    }
}

errno_t DiskController_WriteToPath(DiskControllerRef _Nonnull self, const char* _Nonnull path)
{
    return RamFSContainer_WriteToPath(self->fsContainer, path);
}
