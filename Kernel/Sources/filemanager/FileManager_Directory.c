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
#include <security/SecurityManager.h>
#include <kpi/fcntl.h>


static errno_t _FileManager_SetDirectoryPath(FileManagerRef _Nonnull self, const char* _Nonnull path, InodeRef _Nonnull * _Nonnull pDirToAssign)
{
    decl_try_err();
    ResolvedPath r;

    // Get the inode that represents the new directory
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r));


    // Make sure that it is actually a directory and that we have at least search
    // permission
    Inode_Lock(r.inode);
    if (S_ISDIR(Inode_GetMode(r.inode))) {
        err = SecurityManager_CheckNodeAccess(gSecurityManager, r.inode, self->ruid, self->rgid, X_OK);
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
    return FileHierarchy_GetPath(self->fileHierarchy, self->workingDirectory, self->rootDirectory, self->ruid, self->rgid, pBuffer, bufferSize);
}


// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
errno_t FileManager_CreateDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, mode_t mode)
{
    decl_try_err();
    ResolvedPath r;
    DirectoryEntryInsertionHint dih;
    InodeRef ip = NULL;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r));

    const PathComponent* dirName = &r.lastPathComponent;
    const mode_t dirPerms = ~self->umask & (mode & 0777);


    // Create the new directory and add it to the parent directory if it doesn't
    // exist; otherwise error out
    Inode_Lock(r.inode);
    err = Filesystem_AcquireNodeForName(Inode_GetFilesystem(r.inode), r.inode, dirName, &dih, NULL);
    if (err == ENOENT) {
        // We must have write permissions for the parent directory
        err = SecurityManager_CheckNodeAccess(gSecurityManager, r.inode, self->ruid, self->rgid, W_OK);
        if (err == EOK) {
            err = Filesystem_CreateNode(Inode_GetFilesystem(r.inode), r.inode, dirName, &dih, self->ruid, self->rgid, __S_MKMODE(S_IFDIR, dirPerms), &ip);
        }
    }
    else if (err == EOK) {
        err = EEXIST;
    }
    Inode_Unlock(r.inode);

catch:
    Inode_Relinquish(ip);
    ResolvedPath_Deinit(&r);
    
    return err;
}

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
errno_t FileManager_OpenDirectory(FileManagerRef _Nonnull self, const char* _Nonnull path, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath r;

    *pOutChannel = NULL;
    
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r));

    Inode_Lock(r.inode);
    if (S_ISDIR(Inode_GetMode(r.inode))) {
        err = SecurityManager_CheckNodeAccess(gSecurityManager, r.inode, self->ruid, self->rgid, R_OK);
    }
    else {
        err = ENOTDIR;
    }
    Inode_Unlock(r.inode);
    throw_iferr(err);
    
    err = Inode_CreateChannel(r.inode, O_RDONLY, pOutChannel);
    
catch:
    ResolvedPath_Deinit(&r);
    return err;
}
