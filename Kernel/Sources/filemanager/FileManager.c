//
//  FileManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileManager.h"
#include <filesystem/Filesystem.h>
#include <filesystem/FSUtilities.h>


void FileManager_Init(FileManagerRef _Nonnull self, FileHierarchyRef _Nonnull pFileHierarchy, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask)
{
    self->fileHierarchy = Object_RetainAs(pFileHierarchy, FileHierarchy);
    self->rootDirectory = Inode_Reacquire(pRootDir);
    self->workingDirectory = Inode_Reacquire(pWorkingDir);
    self->umask = umask;
    self->ruid = uid;
    self->rgid = gid;
}

void FileManager_Deinit(FileManagerRef _Nonnull self)
{
    if (self->workingDirectory) {
        Inode_Relinquish(self->workingDirectory);
        self->workingDirectory = NULL;
    }
    if (self->rootDirectory) {
        Inode_Relinquish(self->rootDirectory);
        self->rootDirectory = NULL;
    }

    if (self->fileHierarchy) {
        Object_Release(self->fileHierarchy);
        self->fileHierarchy = NULL;
    }
}

uid_t FileManager_GetRealUserId(FileManagerRef _Nonnull self)
{
    return self->ruid;
}

gid_t FileManager_GetRealGroupId(FileManagerRef _Nonnull self)
{
    return self->rgid;
}

mode_t FileManager_UMask(FileManagerRef _Nonnull self, mode_t mask)
{
    const mode_t omask = self->umask;

    self->umask = mask & 0777;
    return omask;
}
