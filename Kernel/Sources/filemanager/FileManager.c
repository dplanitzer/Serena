//
//  FileManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileManager.h"
#include <filesystem/Filesystem.h>


void FileManager_Init(FileManagerRef _Nonnull self, FileHierarchyRef _Nonnull pFileHierarchy, User user, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, FilePermissions fileCreationMask)
{
    self->fileHierarchy = Object_RetainAs(pFileHierarchy, FileHierarchy);
    self->rootDirectory = Inode_Reacquire(pRootDir);
    self->workingDirectory = Inode_Reacquire(pWorkingDir);
    self->fileCreationMask = fileCreationMask;
    self->realUser = user;
}

void FileManager_Deinit(FileManagerRef _Nonnull self)
{
    Inode_Relinquish(self->workingDirectory);
    self->workingDirectory = NULL;
    Inode_Relinquish(self->rootDirectory);
    self->rootDirectory = NULL;

    Object_Release(self->fileHierarchy);
    self->fileHierarchy = NULL;
}

UserId FileManager_GetRealUserId(FileManagerRef _Nonnull self)
{
    return self->realUser.uid;
}
