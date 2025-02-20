//
//  FileManager_Filesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//
#ifndef __DISKIMAGE__

#include "FileManager.h"
#include "FileHierarchy.h"
#include "FilesystemManager.h"
#include <driver/DriverCatalog.h>
#include <filesystem/Filesystem.h>
#include <filesystem/IOChannel.h>
#include <System/Filesystem.h>


// Discovers and starts the filesystem stored on the disk managed by the driver
// 'diskPath' and returns the filesystem object in 'pOutFs'.
static errno_t discover_and_start_disk_fs(FileManagerRef _Nonnull self, const char* _Nonnull diskPath, void* _Nullable params, size_t paramsSize, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    ResolvedPath rp_disk;
    IOChannelRef devChan = NULL;

    // Resolve the path to the disk device file
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, diskPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_disk));


    // Open the disk driver
    Inode_Lock(rp_disk.inode);
    if (Inode_GetFileType(rp_disk.inode) == kFileType_Device) {
        err = _FileManager_OpenFile(self, rp_disk.inode, kOpen_ReadWrite);
    }
    else {
        err = ENODEV;
    }
    Inode_Unlock(rp_disk.inode);
    throw_iferr(err);


    try(Inode_CreateChannel(rp_disk.inode, kOpen_ReadWrite, &devChan));


    // Start the filesystem
    try(FilesystemManager_DiscoverAndStartFilesystemWithChannel(gFilesystemManager, devChan, params, paramsSize, pOutFs));

catch:
    IOChannel_Release(devChan);
    ResolvedPath_Deinit(&rp_disk);

    return err;
}

// Mounts the filesystem stored in the container at 'containerPath' at the
// directory 'atDirPath'. 'params' are optional mount parameters that are passed
// to the filesystem to mount.
errno_t FileManager_Mount(FileManagerRef _Nonnull self, MountType type, const char* _Nonnull containerPath, const char* _Nonnull atDirPath, const void* _Nullable params, size_t paramsSize)
{
    decl_try_err();
    ResolvedPath rp_atDir;
    FilesystemRef fs = NULL;

    err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, atDirPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_atDir);
    if (err != EOK) {
        return err;
    }


    // Validate the directory where we want to mount
    if (Inode_GetFileType(rp_atDir.inode) != kFileType_Directory) {
        ResolvedPath_Deinit(&rp_atDir);
        return ENOTDIR;
    }
    

    switch (type) {
        case kMount_Disk:
            // Note that this call takes ownership of the inode reference
            err = discover_and_start_disk_fs(self, containerPath, params, paramsSize, &fs);
            break;

        case kMount_DriverCatalog:
            fs = DriverCatalog_CopyChapterFilesystem(gDriverCatalog, containerPath);
            if (fs == NULL) {
                err = ENOENT;
            }
            break;
    }


    // Attach the filesystem
    if (err == EOK) {
        err = FileHierarchy_AttachFilesystem(self->fileHierarchy, fs, rp_atDir.inode);
    }


    ResolvedPath_Deinit(&rp_atDir);
    Object_Release(fs);

    return err;
}

// Unmounts the filesystem mounted at the directory 'atDirPath'.
errno_t FileManager_Unmount(FileManagerRef _Nonnull self, const char* _Nonnull atDirPath, UnmountOptions options)
{
    decl_try_err();
    ResolvedPath rp_atDir;
    bool forced = ((options & kUnmount_Forced) == kUnmount_Forced) ? true : false;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, atDirPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_atDir));


    // Detach the filesystem
    try(FileHierarchy_DetachFilesystemAt(self->fileHierarchy, rp_atDir.inode, forced));

catch:
    ResolvedPath_Deinit(&rp_atDir);

    return err;
}

#endif  /* __DISKIMAGE__ */
