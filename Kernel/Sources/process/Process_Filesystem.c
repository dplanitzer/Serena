//
//  Process_Filesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/FilesystemManager.h>
#include <System/Filesystem.h>


// Mounts the filesystem stored in the container at 'containerPath' at the
// directory 'atDirPath'. 'params' are optional mount parameters that are passed
// to the filesystem to mount.
errno_t Process_Mount(ProcessRef _Nonnull self, const char* _Nonnull containerPath, const char* _Nonnull atDirPath, const void* _Nullable params, size_t paramsSize)
{
    decl_try_err();
    ResolvedPath rp_container;
    ResolvedPath rp_atDir;
    IOChannelRef devChan = NULL;
    FilesystemRef fs = NULL;

    Lock_Lock(&self->lock);
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, containerPath, self->rootDirectory, self->workingDirectory, self->realUser, &rp_container));
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, atDirPath, self->rootDirectory, self->workingDirectory, self->realUser, &rp_atDir));


    // Open the disk driver
    if (Inode_GetFileType(rp_container.inode) != kFileType_Device) {
        throw(ENODEV);
    }

    Inode_Lock(rp_container.inode);
    err = Filesystem_OpenFile(Inode_GetFilesystem(rp_container.inode), rp_container.inode, kOpen_ReadWrite, self->realUser);
    Inode_Unlock(rp_container.inode);
    throw_iferr(err);

    // Note that this call takes ownership of the inode reference
    try(Filesystem_CreateChannel(Inode_GetFilesystem(rp_container.inode), rp_container.inode, kOpen_ReadWrite, &devChan));
    rp_container.inode = NULL;


    // Validate the directory where we want to mount
    if (Inode_GetFileType(rp_atDir.inode) != kFileType_Directory) {
        throw(ENOTDIR);
    }


    // Start the filesystem
    try(FilesystemManager_DiscoverAndStartFilesystemWithChannel(gFilesystemManager, devChan, params, paramsSize, &fs));


    // Attach the filesystem
    try(FileHierarchy_AttachFilesystem(self->fileHierarchy, fs, rp_atDir.inode));

catch:
    ResolvedPath_Deinit(&rp_atDir);
    ResolvedPath_Deinit(&rp_container);

    Lock_Unlock(&self->lock);

    IOChannel_Release(devChan);
    Object_Release(fs);

    return err;
}

// Unmounts the filesystem mounted at the directory 'atDirPath'.
errno_t Process_Unmount(ProcessRef _Nonnull self, const char* _Nonnull atDirPath, uint32_t options)
{
    decl_try_err();
    ResolvedPath rp_atDir;
    bool forced = ((options & kUnmount_Forced) == kUnmount_Forced) ? true : false;

    Lock_Lock(&self->lock);
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, atDirPath, self->rootDirectory, self->workingDirectory, self->realUser, &rp_atDir));


    // Detach the filesystem
    try(FileHierarchy_DetachFilesystemAt(self->fileHierarchy, rp_atDir.inode, forced));

catch:
    ResolvedPath_Deinit(&rp_atDir);

    Lock_Unlock(&self->lock);

    return err;
}
