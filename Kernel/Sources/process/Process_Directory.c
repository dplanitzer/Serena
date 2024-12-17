//
//  Process_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/DirectoryChannel.h>


// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
errno_t Process_SetRootDirectoryPath(ProcessRef _Nonnull self, const char* _Nonnull path)
{
    Lock_Lock(&self->lock);
    const errno_t err = FileManager_SetRootDirectoryPath(&self->fm, path);
    Lock_Unlock(&self->lock);

    return err;
}

// Sets the receiver's current working directory to the given path.
errno_t Process_SetWorkingDirectoryPath(ProcessRef _Nonnull self, const char* _Nonnull path)
{
    Lock_Lock(&self->lock);
    const errno_t err = FileManager_SetWorkingDirectoryPath(&self->fm, path);
    Lock_Unlock(&self->lock);

    return err;
}

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
errno_t Process_GetWorkingDirectoryPath(ProcessRef _Nonnull self, char* _Nonnull pBuffer, size_t bufferSize)
{
    Lock_Lock(&self->lock);
    const errno_t err = FileManager_GetWorkingDirectoryPath(&self->fm, pBuffer, bufferSize);
    Lock_Unlock(&self->lock);

    return err;
}

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
errno_t Process_CreateDirectory(ProcessRef _Nonnull self, const char* _Nonnull path, FilePermissions permissions)
{
    Lock_Lock(&self->lock);
    const errno_t err = FileManager_CreateDirectory(&self->fm, path, permissions);
    Lock_Unlock(&self->lock);
    return err;
}

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
errno_t Process_OpenDirectory(ProcessRef _Nonnull self, const char* _Nonnull path, int* _Nonnull pOutIoc)
{
    decl_try_err();
    IOChannelRef chan = NULL;

    Lock_Lock(&self->lock);
    err = FileManager_OpenDirectory(&self->fm, path, &chan);
    try(IOChannelTable_AdoptChannel(&self->ioChannelTable, chan, pOutIoc));
    chan = NULL;
    
catch:
    Lock_Unlock(&self->lock);
    if (err != EOK) {
        IOChannel_Release(chan);
        *pOutIoc = -1;
    }
    return err;
}
