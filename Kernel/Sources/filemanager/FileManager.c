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

GroupId FileManager_GetRealGroupId(FileManagerRef _Nonnull self)
{
    return self->realUser.gid;
}

// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
FilePermissions FileManager_GetFileCreationMask(FileManagerRef _Nonnull self)
{
    return self->fileCreationMask;
}

// Sets the file creation mask of the receiver.
void FileManager_SetFileCreationMask(FileManagerRef _Nonnull self, FilePermissions mask)
{
    self->fileCreationMask = mask & 0777;
}
