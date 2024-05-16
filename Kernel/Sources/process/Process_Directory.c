//
//  Process_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/FilesystemManager.h>
#include <filesystem/DirectoryChannel.h>


static errno_t Process_SetDirectoryPath_Locked(ProcessRef _Nonnull self, const char* _Nonnull pPath, InodeRef _Nonnull * _Nonnull pDirToAssign)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;

    Process_MakePathResolver(self, &pr);

    // Get the inode that represents the new directory
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOnly, pPath, &r));


    // Make sure that it is actually a directory
    if (!Inode_IsDirectory(r.target)) {
        throw(ENOTDIR);
    }


    // Make sure that we do have search permission on the last path component (directory)
    try(Filesystem_CheckAccess(Inode_GetFilesystem(r.target), r.target, pr.user, kFilePermission_Execute));


    // Remember the new inode as our new directory
    if (Inode_GetId(*pDirToAssign) != Inode_GetId(r.target)) {
        Inode_Relinquish(*pDirToAssign);
        *pDirToAssign = r.target;
        r.target = NULL;
    }

catch:
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    return err;
}

// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
errno_t Process_SetRootDirectoryPath(ProcessRef _Nonnull pProc, const char* pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = Process_SetDirectoryPath_Locked(pProc, pPath, &pProc->rootDirectory);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Sets the receiver's current working directory to the given path.
errno_t Process_SetWorkingDirectoryPath(ProcessRef _Nonnull pProc, const char* _Nonnull pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = Process_SetDirectoryPath_Locked(pProc, pPath, &pProc->workingDirectory);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
errno_t Process_GetWorkingDirectoryPath(ProcessRef _Nonnull pProc, char* _Nonnull pBuffer, size_t bufferSize)
{
    PathResolver pr;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    const errno_t err = PathResolver_GetDirectoryPath(&pr, pr.workingDirectory, pBuffer, bufferSize);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
errno_t Process_CreateDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FilePermissions permissions)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    InodeRef pDirNode = NULL;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOrParent, pPath, &r));
    if (r.target != NULL) {
        throw(EEXIST);
    }
    FilesystemRef pFS = Inode_GetFilesystem(r.parent);
    FilePermissions dirPerms = ~pProc->fileCreationMask & (permissions & 0777);


    // Create the new directory and add it to its parent directory
    try(Filesystem_CreateNode(pFS, kFileType_Directory, pr.user, dirPerms, r.parent, &r.lastPathComponent, &r.insertionHint, &pDirNode));

catch:
    Inode_Relinquish(pDirNode);
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
errno_t Process_OpenDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, int* _Nonnull pOutIoc)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    IOChannelRef pDir = NULL;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOnly, pPath, &r));
    try(Filesystem_OpenDirectory(Inode_GetFilesystem(r.target), r.target, pr.user));
    // Note that this method takes ownership of the inode reference
    try(DirectoryChannel_Create(r.target, &pDir));
    r.target = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pDir, pOutIoc));
    pDir = NULL;
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    IOChannel_Release(pDir);
    *pOutIoc = -1;
    return err;
}
