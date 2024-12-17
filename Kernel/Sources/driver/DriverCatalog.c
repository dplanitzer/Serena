//
//  DriverCatalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//


#include "DriverCatalog.h"
#include <filemanager/FileHierarchy.h>
#include <filesystem/devfs/DevFS.h>


typedef struct DriverCatalog {
    DevFSRef _Nonnull           devfs;
    FileHierarchyRef _Nonnull   fh;
    InodeRef _Nonnull           rootDirectory;
} DriverCatalog;


DriverCatalogRef _Nonnull  gDriverCatalog;

errno_t DriverCatalog_Create(DriverCatalogRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverCatalogRef self;
    
    try(kalloc_cleared(sizeof(DriverCatalog), (void**) &self));
    
    try(DevFS_Create(&self->devfs));
    try(Filesystem_Start(self->devfs, NULL, 0));
    try(FileHierarchy_Create((FilesystemRef)self->devfs, &self->fh));
    try(Filesystem_AcquireRootDirectory(self->devfs, &self->rootDirectory));

    *pOutSelf = self;
    return EOK;

catch:
    DriverCatalog_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void DriverCatalog_Destroy(DriverCatalogRef _Nullable self)
{
    if (self) {
        Inode_Relinquish(self->rootDirectory);
        self->rootDirectory = NULL;

        Object_Release(self->fh);
        self->fh = NULL;

        Filesystem_Stop(self->devfs);
        Object_Release(self->devfs);
        self->devfs = NULL;

        kfree(self);
    }
}

DevFSRef _Nonnull DriverCatalog_GetDevicesFilesystem(DriverCatalogRef _Nonnull self)
{
    return self->devfs;
}

errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverRef _Nonnull driver, intptr_t arg, DriverCatalogId* _Nonnull pOutDriverCatalogId)
{
    decl_try_err();
    InodeRef rootDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutDriverCatalogId = kDriverCatalogId_None;

    pc.name = name;
    pc.count = String_Length(name);

    const FilePermissions ownerPerms = kFilePermission_Read | kFilePermission_Write;
    const FilePermissions otherPerms = kFilePermission_Read | kFilePermission_Write;
    const FilePermissions permissions = FilePermissions_Make(ownerPerms, otherPerms, otherPerms);

    try(Filesystem_AcquireRootDirectory(self->devfs, &rootDir));
    try(DevFS_CreateDevice(self->devfs, kUser_Root, permissions, rootDir, &pc, driver, arg, &pNode));
    *pOutDriverCatalogId = (DriverCatalogId)Inode_GetId(pNode);

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(rootDir);

    return err;
}

errno_t DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverCatalogId driverCatalogId)
{
    decl_try_err();
    InodeRef rootDir = NULL;
    InodeRef pNode = NULL;

    if (driverCatalogId == kDriverCatalogId_None) {
        return EOK;
    }
    
    try(Filesystem_AcquireRootDirectory(self->devfs, &rootDir));
    try(Filesystem_AcquireNodeWithId((FilesystemRef)self->devfs, (InodeId)driverCatalogId, &pNode));
    try(Filesystem_Unlink(self->devfs, pNode, rootDir, kUser_Root));

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(rootDir);

    return err;
}

errno_t DriverCatalog_IsDriverPublished(DriverCatalogRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    ResolvedPath rp;

    err = FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, kUser_Root, &rp);
    ResolvedPath_Deinit(&rp);

    return err;
}

errno_t DriverCatalog_OpenDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath rp;

    try(FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, kUser_Root, &rp));
    try(Filesystem_CreateChannel(self->devfs, rp.inode, mode, pOutChannel));
    rp.inode = NULL;    // Consumed by CreateChannel()

catch:
    ResolvedPath_Deinit(&rp);
    return err;
}
