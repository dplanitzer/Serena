//
//  Process_FileManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FileChannel.h>


//
// MARK: Directory
//

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
errno_t Process_CreateDirectory(ProcessRef _Nonnull self, const char* _Nonnull path, mode_t permissions)
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
    IOChannelRef chan;

    Lock_Lock(&self->lock);
    err = FileManager_OpenDirectory(&self->fm, path, &chan);
    if (err == EOK) {
        err = IOChannelTable_AdoptChannel(&self->ioChannelTable, chan, pOutIoc);
    }
    Lock_Unlock(&self->lock);
    
    if (err != EOK) {
        IOChannel_Release(chan);
        *pOutIoc = -1;
    }
    return err;
}


//
// MARK: File
//

mode_t Process_UMask(ProcessRef _Nonnull self, mode_t mask)
{
    mode_t omask;

    Lock_Lock(&self->lock);
    if (mask != SEO_UMASK_NO_CHANGE) {
        omask = FileManager_UMask(&self->fm, mask);
    }
    else {
        omask = FileManager_GetUMask(&self->fm);
    }
    Lock_Unlock(&self->lock);
    
    return omask;
}

// Creates a file in the given filesystem location.
errno_t Process_CreateFile(ProcessRef _Nonnull self, const char* _Nonnull path, unsigned int mode, mode_t permissions, int* _Nonnull pOutIoc)
{
    decl_try_err();
    IOChannelRef chan;

    Lock_Lock(&self->lock);
    err = FileManager_CreateFile(&self->fm, path, mode, permissions, &chan);
    if (err == EOK) {
        err = IOChannelTable_AdoptChannel(&self->ioChannelTable, chan, pOutIoc);
    }
    Lock_Unlock(&self->lock);

    if (err != EOK) {
        IOChannel_Release(chan);
        *pOutIoc = -1;
    }
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t Process_OpenFile(ProcessRef _Nonnull self, const char* _Nonnull path, unsigned int mode, int* _Nonnull pOutIoc)
{
    decl_try_err();
    IOChannelRef chan;

    Lock_Lock(&self->lock);
    err = FileManager_OpenFile(&self->fm, path, mode, &chan);
    if (err == EOK) {
        err = IOChannelTable_AdoptChannel(&self->ioChannelTable, chan, pOutIoc);
    }
    Lock_Unlock(&self->lock);

    if (err != EOK) {
        IOChannel_Release(chan);
        *pOutIoc = -1;
    }
    return err;
}

// Returns information about the file at the given path.
errno_t Process_GetFileInfo(ProcessRef _Nonnull self, const char* _Nonnull path, struct stat* _Nonnull pOutInfo)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_GetFileInfo(&self->fm, path, pOutInfo);
    Lock_Unlock(&self->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_GetFileInfo_ioc(ProcessRef _Nonnull self, int ioc, struct stat* _Nonnull pOutInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        err = FileManager_GetFileInfo_ioc(&self->fm, pChannel, pOutInfo);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

errno_t Process_SetFileMode(ProcessRef _Nonnull self, const char* _Nonnull path, mode_t mode)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_SetFileMode(&self->fm, path, mode);
    Lock_Unlock(&self->lock);
    
    return err;
}

errno_t Process_SetFileOwner(ProcessRef _Nonnull self, const char* _Nonnull path, uid_t uid, gid_t gid)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_SetFileOwner(&self->fm, path, uid, gid);
    Lock_Unlock(&self->lock);
    
    return err;
}

errno_t Process_SetFileTimestamps(ProcessRef _Nonnull self, const char* _Nonnull path, const struct timespec times[_Nullable 2])
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_SetFileTimestamps(&self->fm, path, times);
    Lock_Unlock(&self->lock);
    
    return err;
}

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
errno_t Process_TruncateFile(ProcessRef _Nonnull self, const char* _Nonnull path, off_t length)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_TruncateFile(&self->fm, path, length);
    Lock_Unlock(&self->lock);

    return err;
}

// Same as above but the file is identified by the given I/O channel.
errno_t Process_TruncateFile_ioc(ProcessRef _Nonnull self, int ioc, off_t length)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        err = FileManager_TruncateFile_ioc(&self->fm, pChannel, length);
        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
errno_t Process_CheckAccess(ProcessRef _Nonnull self, const char* _Nonnull path, int mode)
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


//
// MARK: Filesystem
//

// Mounts the filesystem stored in the container at 'containerPath' at the
// directory 'atDirPath'. 'params' are optional mount parameters that are passed
// to the filesystem to mount.
errno_t Process_Mount(ProcessRef _Nonnull self, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_Mount(&self->fm, objectType, objectName, atDirPath, params);
    Lock_Unlock(&self->lock);

    return err;
}

// Unmounts the filesystem mounted at the directory 'atDirPath'.
errno_t Process_Unmount(ProcessRef _Nonnull self, const char* _Nonnull atDirPath, UnmountOptions options)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_Unmount(&self->fm, atDirPath, options);
    Lock_Unlock(&self->lock);

    return err;
}

errno_t Process_GetFilesystemDiskPath(ProcessRef _Nonnull self, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_GetFilesystemDiskPath(&self->fm, fsid, buf, bufSize);
    Lock_Unlock(&self->lock);

    return err;
}
