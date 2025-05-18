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
#include <Catalog.h>
#include <filesystem/Filesystem.h>
#include <filesystem/IOChannel.h>
#include <kern/string.h>
#include <kpi/fcntl.h>
#include <kpi/fs.h>


// Establishes and starts the filesystem stored on the disk managed by the disk
// driver 'diskPath' and returns the filesystem object in 'pOutFs'.
static errno_t establish_and_start_disk_fs(FileManagerRef _Nonnull self, const char* _Nonnull fsName, const char* _Nonnull diskPath, const char* _Nonnull params, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    const unsigned int mode = O_RDWR;
    FilesystemRef fs = NULL;
    ResolvedPath rp_disk;

    // SeFS only for now
    if (!String_Equals(fsName, kMount_SeFS)) {
        throw(EINVAL);
    }

    
    // Resolve the path to the disk device file
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, diskPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_disk));


    // Open the disk driver and establish the filesystem
    Inode_Lock(rp_disk.inode);
    if (S_ISDEV(Inode_GetMode(rp_disk.inode))) {
        err = _FileManager_OpenFile(self, rp_disk.inode, mode);
        if (err == EOK) {
            err = FilesystemManager_EstablishFilesystem(gFilesystemManager, rp_disk.inode, mode, &fs);
        }
    }
    else {
        err = ENODEV;
    }
    Inode_Unlock(rp_disk.inode);
    throw_iferr(err);


    // Start the filesystem
    try(FilesystemManager_StartFilesystem(gFilesystemManager, fs, params));

catch:
    ResolvedPath_Deinit(&rp_disk);
    *pOutFs = fs;
    
    return err;
}

static errno_t lookup_catalog(FileManagerRef _Nonnull self, const char* _Nonnull catalogName, FilesystemRef _Nullable * _Nonnull pOutFs)
{
    decl_try_err();
    CatalogRef catalog = NULL;

    if (String_Equals(catalogName, kCatalogName_Drivers)) {
        catalog = gDriverCatalog;
    }
    else if (String_Equals(catalogName, kCatalogName_Filesystems)) {
        catalog = gFSCatalog;
    }
    else if (String_Equals(catalogName, kCatalogName_Processes)) {
        catalog = gProcCatalog;
    }
    else {
        *pOutFs = NULL;
        return ENOENT;
    }

    *pOutFs = Catalog_CopyFilesystem(catalog);
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
    if (!S_ISDIR(Inode_GetMode(rp_atDir.inode))) {
        ResolvedPath_Deinit(&rp_atDir);
        return ENOTDIR;
    }
    

    if (String_Equals(objectType, kMount_Catalog)) {
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
errno_t FileManager_Unmount(FileManagerRef _Nonnull self, const char* _Nonnull atDirPath, UnmountOptions options)
{
    decl_try_err();
    ResolvedPath rp_atDir;
    bool forced = ((options & kUnmount_Forced) == kUnmount_Forced) ? true : false;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, atDirPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &rp_atDir));


    // Detach the filesystem. Note that this call consumes rp_atDir.inode
    try(FileHierarchy_DetachFilesystemAt(self->fileHierarchy, rp_atDir.inode, forced));

catch:

return err;
}

errno_t FileManager_GetFilesystemDiskPath(FileManagerRef _Nonnull self, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    InodeRef ip = NULL;

    err = FilesystemManager_AcquireDriverNodeForFsid(gFilesystemManager, fsid, &ip);
    if (err == EOK) {
        //XXX getting insufficient permissions if using the user credentials 
        err = FileHierarchy_GetPath(self->fileHierarchy, ip, self->rootDirectory, kUserId_Root, kGroupId_Root /*self->ruid, self->rgid*/, buf, bufSize);
        Inode_Relinquish(ip);
    }
    else if (Catalog_IsFsid(gDriverCatalog, fsid)) {
        err = Catalog_GetName(gDriverCatalog, buf, bufSize);
    }
    else if (Catalog_IsFsid(gFSCatalog, fsid)) {
        err = Catalog_GetName(gFSCatalog, buf, bufSize);
    }
    else if (Catalog_IsFsid(gProcCatalog, fsid)) {
        err = Catalog_GetName(gProcCatalog, buf, bufSize);
    }
    else {
        if (bufSize < 1) {
            err = ERANGE;
        }
        else {
            *buf = '\0';
        }
    }

    return err;
}

#endif  /* __DISKIMAGE__ */
