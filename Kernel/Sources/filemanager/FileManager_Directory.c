//
//  FileManager_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileManager.h"
#include "FileHierarchy.h"
#include <filesystem/DirectoryChannel.h>


static errno_t _FileManager_SetDirectoryPath(FileManagerRef _Nonnull self, const char* _Nonnull path, InodeRef _Nonnull * _Nonnull pDirToAssign)
{
    decl_try_err();
    ResolvedPath r;

    // Get the inode that represents the new directory
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->realUser, &r));

    Inode_Lock(r.inode);

    // Make sure that it is actually a directory and that we have at least search
    // permission
    if (Inode_IsDirectory(r.inode)) {
        err = Filesystem_CheckAccess(Inode_GetFilesystem(r.inode), r.inode, self->realUser, kAccess_Searchable);
    }
    else {
        err = ENOTDIR;
    }

    Inode_Unlock(r.inode);

    if (err == EOK) {
        // Remember the new inode as our new directory
        Inode_Relinquish(*pDirToAssign);
        *pDirToAssign = r.inode;
        r.inode = NULL;
    }

catch:
    ResolvedPath_Deinit(&r);
    return err;
}

// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
errno_t FileManager_SetRootDirectoryPath(FileManagerRef _Nonnull self, const char* _Nonnull path)
{
    return _FileManager_SetDirectoryPath(self, path, &self->rootDirectory);
}

// Sets the receiver's current working directory to the given path.
errno_t FileManager_SetWorkingDirectoryPath(FileManagerRef _Nonnull self, const char* _Nonnull path)
{
    return _FileManager_SetDirectoryPath(self, path, &self->workingDirectory);
}

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
errno_t FileManager_GetWorkingDirectoryPath(FileManagerRef _Nonnull self, char* _Nonnull pBuffer, size_t bufferSize)
{
    return FileHierarchy_GetDirectoryPath(self->fileHierarchy, self->workingDirectory, self->rootDirectory, self->realUser, pBuffer, bufferSize);
}


// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
errno_t FileManager_CreateDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, FilePermissions permissions)
{
    decl_try_err();
    ResolvedPath r;
    DirectoryEntryInsertionHint dih;
    InodeRef parentDir = NULL;
    InodeRef newDir = NULL;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, path, self->rootDirectory, self->workingDirectory, self->realUser, &r));

    const PathComponent* pName = &r.lastPathComponent;
    const FilePermissions dirPerms = ~self->fileCreationMask & (permissions & 0777);
    FilesystemRef fs = Inode_GetFilesystem(r.inode);
    parentDir = r.inode;
    r.inode = NULL;
    Inode_Lock(parentDir);


    // Can not create a directory with names . or ..
    if ((pName->count == 1 && pName->name[0] == '.')
        || (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.')) {
        throw(EEXIST);
    }


    // Create the new directory and add it to the parent directory if it doesn't
    // exist; otherwise error out
    err = Filesystem_AcquireNodeForName(fs, parentDir, pName, self->realUser, &dih, NULL);
    if (err == ENOENT) {
        try(Filesystem_CreateNode(fs, kFileType_Directory, self->realUser, dirPerms, parentDir, pName, &dih, &newDir));
    }
    else if (err == EOK) {
        throw(EEXIST);
    }

catch:
    Inode_Relinquish(newDir);
    Inode_UnlockRelinquish(parentDir);

    ResolvedPath_Deinit(&r);
    
    return err;
}

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
errno_t FileManager_OpenDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath r;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->realUser, &r));

    Inode_Lock(r.inode);
    if (Inode_GetFileType(r.inode) == kFileType_Directory) {
        err = Filesystem_CheckAccess(Inode_GetFilesystem(r.inode), r.inode, self->realUser, kAccess_Readable);
    }
    else {
        err = ENOTDIR;
    }
    Inode_Unlock(r.inode);
    throw_iferr(err);
    
    // Note that this method takes ownership of the inode reference
    try(Filesystem_CreateChannel(Inode_GetFilesystem(r.inode), r.inode, kOpen_Read, pOutChannel));
    r.inode = NULL;
    
catch:
    ResolvedPath_Deinit(&r);
    return err;
}
