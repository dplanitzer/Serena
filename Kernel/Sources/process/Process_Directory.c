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


// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
errno_t Process_SetRootDirectoryPath(ProcessRef _Nonnull pProc, const char* pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_SetRootDirectoryPath(pProc->pathResolver, pProc->realUser, pPath);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Sets the receiver's current working directory to the given path.
errno_t Process_SetWorkingDirectoryPath(ProcessRef _Nonnull pProc, const char* _Nonnull pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_SetWorkingDirectoryPath(pProc->pathResolver, pProc->realUser, pPath);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
errno_t Process_GetWorkingDirectoryPath(ProcessRef _Nonnull pProc, char* _Nonnull pBuffer, size_t bufferSize)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_GetWorkingDirectoryPath(pProc->pathResolver, pProc->realUser, pBuffer, bufferSize);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
errno_t Process_CreateDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FilePermissions permissions)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);

    if ((err = PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_CreateDirectory(r.filesystem, &r.lastPathComponent, r.inode, pProc->realUser, ~pProc->fileCreationMask & (permissions & 0777));
    }
    PathResolverResult_Deinit(&r);

    Lock_Unlock(&pProc->lock);

    return err;
}

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
errno_t Process_OpenDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, int* _Nonnull pOutIoc)
{
    decl_try_err();
    PathResolverResult r;
    IOChannelRef pDir = NULL;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(Filesystem_OpenDirectory(r.filesystem, r.inode, pProc->realUser));
    // Note that this method takes ownership of the inode reference
    try(DirectoryChannel_Create(r.inode, &pDir));
    r.inode = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pDir, pOutIoc));
    pDir = NULL;
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    IOChannel_Release(pDir);
    *pOutIoc = -1;
    return err;
}
