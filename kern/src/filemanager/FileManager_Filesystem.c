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
#include <string.h>
#include <driver/disk/DiskDriver.h>
#include <driver/IOCatalog.h>
#include <filesystem/DiskContainer.h>
#include <filesystem/serenafs/SerenaFS.h>
#include <handler/Handler.h>
#include <kpi/file.h>
#include <kpi/filesystem.h>
#include <process/ProcessManager.h>


static errno_t create_disk_fs(FileManagerRef _Nonnull self, InodeRef _Nonnull _Locked driverNode, unsigned int mode, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    FSContainerRef fsContainer = NULL;
    FilesystemRef fs = NULL;

    // We call open() here to ensure that the node is valid and that the user
    // has the necessary permissions to access it
    try(_FileManager_OpenFile(self, driverNode, mode));


    // Get the disk driver that is associated with the inode
    DiskDriverRef disk = dynamiccast(Inode_GetResource(driverNode), DiskDriver);
    if (disk == NULL) {
        throw(ENODEV);
    }


    // Create a disk container and the filesystem
    try(DiskContainer_Create(disk, mode, &fsContainer));
    try(SerenaFS_Create(fsContainer, (SerenaFSRef*)&fs));

catch:
    Object_Release(fsContainer);

    *pOutFs = fs;
    return err;
}

// Establishes and starts the filesystem stored on the disk managed by the disk
// driver 'diskPath' and returns the filesystem object in 'pOutFs'.
static errno_t establish_and_start_disk_fs(FileManagerRef _Nonnull self, const char* _Nonnull fsName, const char* _Nonnull diskPath, const char* _Nonnull params, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    const unsigned int mode = O_RDWR;
    FilesystemRef fs = NULL;
    ResolvedPath rp_disk;

    // SeFS only for now
    if (strcmp(fsName, FS_MOUNT_SEFS)) {
        throw(EINVAL);
    }

    
    // Resolve the path to the disk device file
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, diskPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_disk));


    // Open the disk driver and establish the filesystem
    Inode_Lock(rp_disk.inode);
    if (Inode_IsDevice(rp_disk.inode)) {
        err = create_disk_fs(self, rp_disk.inode, mode, &fs);
    }
    else {
        err = ENODEV;
    }
    Inode_Unlock(rp_disk.inode);
    throw_iferr(err);


    // Start the filesystem
    try(FilesystemManager_RegisterFilesystem(gFilesystemManager, fs));
    try(Filesystem_Start(fs, params));

catch:
    ResolvedPath_Deinit(&rp_disk);
    *pOutFs = fs;
    
    return err;
}

static errno_t lookup_catalog(FileManagerRef _Nonnull self, const char* _Nonnull catalogName, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    FilesystemRef fs = NULL;

    if (!strcmp(catalogName, FS_CATALOG_DEV)) {
        fs = IOCatalog_GetFilesystem(gIOCatalog);
    }
    else {
        *pOutFs = NULL;
        return ENOFS;
    }

    *pOutFs = Object_Retain(fs);
    return EOK;
}

// Mounts the object 'objectName' of type 'type' at the directory 'atDirPath'.
// 'params' are optional mount parameters that are passed to the filesystem to
// mount.
errno_t FileManager_Mount(FileManagerRef _Nonnull self, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    decl_try_err();
    ResolvedPath rp_atDir;
    FilesystemRef fs = NULL;

    err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, atDirPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_atDir);
    if (err != EOK) {
        return err;
    }


    // Validate the directory where we want to mount
    if (!Inode_IsDirectory(rp_atDir.inode)) {
        ResolvedPath_Deinit(&rp_atDir);
        return ENOTDIR;
    }
    

    if (!strcmp(objectType, FS_MOUNT_CATALOG)) {
        err = lookup_catalog(self, objectName, &fs);
    }
    else {
        err = establish_and_start_disk_fs(self, objectType, objectName, params, &fs);
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
errno_t FileManager_Unmount(FileManagerRef _Nonnull self, const char* _Nonnull atDirPath, int flags)
{
    decl_try_err();
    ResolvedPath rp_atDir;
    bool forced = ((flags & FS_UNMOUNT_FORCE) == FS_UNMOUNT_FORCE) ? true : false;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, atDirPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_atDir));


    // Detach the filesystem. Note that this call consumes rp_atDir.inode
    try(FileHierarchy_DetachFilesystemAt(self->fileHierarchy, rp_atDir.inode, forced));

catch:

return err;
}

errno_t FileManager_GetPathForDriver(FileManagerRef _Nonnull self, DriverRef _Nonnull driver, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    InodeRef ip = NULL;

    try(IOCatalog_AcquireNodeForDriver(gIOCatalog, driver, &ip));
    
    //XXX getting insufficient permissions when using the user credentials 
    try(FileHierarchy_GetPath(self->fileHierarchy, ip, self->rootDirectory, UID_ROOT, GID_ROOT /*self->ruid, self->rgid*/, buf, bufSize));

catch:
    Inode_Relinquish(ip);

    return err;
}

#endif  /* __DISKIMAGE__ */
