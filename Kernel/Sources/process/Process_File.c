//
//  Process_File.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FileChannel.h>


// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
FilePermissions Process_GetFileCreationMask(ProcessRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const FilePermissions mask = FileManager_GetFileCreationMask(&self->fm);
    Lock_Unlock(&self->lock);
    return mask;
}

// Sets the file creation mask of the receiver.
void Process_SetFileCreationMask(ProcessRef _Nonnull self, FilePermissions mask)
{
    Lock_Lock(&self->lock);
    FileManager_SetFileCreationMask(&self->fm, mask);
    Lock_Unlock(&self->lock);
}

// Creates a file in the given filesystem location.
errno_t Process_CreateFile(ProcessRef _Nonnull self, const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull pOutIoc)
{
    decl_try_err();
    IOChannelRef chan = NULL;

    Lock_Lock(&self->lock);
    err = FileManager_CreateFile(&self->fm, path, mode, permissions, &chan);
    try(IOChannelTable_AdoptChannel(&self->ioChannelTable, chan, pOutIoc));
    chan = NULL;
    Lock_Unlock(&self->lock);
    return EOK;

catch:
    Lock_Unlock(&self->lock);
    
    IOChannel_Release(chan);
    *pOutIoc = -1;
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t Process_OpenFile(ProcessRef _Nonnull self, const char* _Nonnull path, unsigned int mode, int* _Nonnull pOutIoc)
{
    decl_try_err();
    IOChannelRef chan = NULL;

    Lock_Lock(&self->lock);
    err = FileManager_OpenFile(&self->fm, path, mode, &chan);
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

// Returns information about the file at the given path.
errno_t Process_GetFileInfo(ProcessRef _Nonnull self, const char* _Nonnull path, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_GetFileInfo(&self->fm, path, pOutInfo);
    Lock_Unlock(&self->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_GetFileInfoFromIOChannel(ProcessRef _Nonnull self, int ioc, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        err = FileManager_GetFileInfoFromIOChannel(&self->fm, pChannel, pOutInfo);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

// Modifies information about the file at the given path.
errno_t Process_SetFileInfo(ProcessRef _Nonnull self, const char* _Nonnull path, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_SetFileInfo(&self->fm, path, pInfo);
    Lock_Unlock(&self->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_SetFileInfoFromIOChannel(ProcessRef _Nonnull self, int ioc, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        err = FileManager_SetFileInfoFromIOChannel(&self->fm, pChannel, pInfo);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }

    return err;
}

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
errno_t Process_TruncateFile(ProcessRef _Nonnull self, const char* _Nonnull path, FileOffset length)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_TruncateFile(&self->fm, path, length);
    Lock_Unlock(&self->lock);

    return err;
}

// Same as above but the file is identified by the given I/O channel.
errno_t Process_TruncateFileFromIOChannel(ProcessRef _Nonnull self, int ioc, FileOffset length)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        err = FileManager_TruncateFileFromIOChannel(&self->fm, pChannel, length);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
errno_t Process_CheckAccess(ProcessRef _Nonnull self, const char* _Nonnull path, AccessMode mode)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_CheckAccess(&self->fm, path, mode);
    Lock_Unlock(&self->lock);

    return err;
}

// Unlinks the inode at the path 'path'.
errno_t Process_Unlink(ProcessRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_Unlink(&self->fm, path);
    Lock_Unlock(&self->lock);

    return err;
}

// Renames the file or directory at 'oldPath' to the new location 'newPath'.
errno_t Process_Rename(ProcessRef _Nonnull self, const char* oldPath, const char* newPath)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_Rename(&self->fm, oldPath, newPath);
    Lock_Unlock(&self->lock);

    return err;
}
