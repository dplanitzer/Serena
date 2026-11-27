//
//  FSManager.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "dd.h"
#include "dd_priv.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <filemanager/FileHierarchy.h>
#include <filesystem/FSUtilities.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <kpi/fs_perms.h>


dd_fs_t dd_mount(const char *path)
{
    decl_try_err();
    dd_fs_t self = NULL;
    InodeRef rootDir = NULL;
    FileHierarchyRef fh = NULL;
    size_t pathLen = (path) ? strlen(path) : 0;

    if (pathLen == 0) {
        errno = EINVAL;
        return NULL;
    }

    self = calloc(1, sizeof(struct dd_fs));
    if (self == NULL) {
        return NULL;
    }

    try_null(self->dmgPath, malloc(pathLen + 1), errno);
    strcpy(self->dmgPath, path);
    try(RamContainer_CreateWithContentsOfPath(path, &self->fsContainer));
    try(SerenaFS_Create((FSContainerRef)self->fsContainer, (SerenaFSRef*)&self->fs));
    try(Filesystem_Start(self->fs, ""));

    try(FileHierarchy_Create(self->fs, &fh));
    rootDir = FileHierarchy_AcquireRootDirectory(fh);
    FileManager_Init(&self->fm, fh, UID_ROOT, GID_ROOT, rootDir, rootDir, fs_perms_from_octal(0));
    self->isFmUp = true;

    Inode_Relinquish(rootDir);
    Object_Release(fh);

    return self;

catch:
    Object_Release(fh);
    Inode_Relinquish(rootDir);
    dd_unmount(self, 0);

    return NULL;
}

int dd_unmount(dd_fs_t self, int flags)
{
    errno_t err = EOK;

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

        if (self->fsContainer) {
            if ((flags & DD_USYNC) == DD_USYNC) {
                err = RamContainer_WriteToPath(self->fsContainer, self->dmgPath);
            }
            Object_Release(self->fsContainer);
            self->fsContainer = NULL;
        }

        free(self->dmgPath);
        free(self);
    }


    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_sync(dd_fs_t fs)
{
    const errno_t err = RamContainer_WriteToPath(fs->fsContainer, fs->dmgPath);

    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}
