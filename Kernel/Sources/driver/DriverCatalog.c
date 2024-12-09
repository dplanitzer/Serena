//
//  DriverCatalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//


#include "DriverCatalog.h"
#include <filesystem/devfs/DevFS.h>


typedef struct DriverCatalog {
    DevFSRef _Nonnull   devfs;
} DriverCatalog;


DriverCatalogRef _Nonnull  gDriverCatalog;

errno_t DriverCatalog_Create(DriverCatalogRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverCatalogRef self;
    
    try(kalloc_cleared(sizeof(DriverCatalog), (void**) &self));
    
    try(DevFS_Create(&self->devfs));
    try(Filesystem_Start(self->devfs, NULL, 0));

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

errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverRef _Nonnull driver, DriverId* _Nonnull pOutDriverId)
{
    decl_try_err();
    InodeRef rootDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutDriverId = kDriverId_None;

    pc.name = name;
    pc.count = String_Length(name);

    const FilePermissions ownerPerms = kFilePermission_Read | kFilePermission_Write;
    const FilePermissions otherPerms = kFilePermission_Read | kFilePermission_Write;
    const FilePermissions permissions = FilePermissions_Make(ownerPerms, otherPerms, otherPerms);

    try(Filesystem_AcquireRootDirectory(self->devfs, &rootDir));
    try(DevFS_CreateDevice(self->devfs, kUser_Root, permissions, rootDir, &pc, driver, &pNode));
    *pOutDriverId = (DriverId)Inode_GetId(pNode);

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(rootDir);

    return err;
}

errno_t DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverId driverId)
{
    decl_try_err();
    InodeRef rootDir = NULL;
    InodeRef pNode = NULL;

    try(Filesystem_AcquireRootDirectory(self->devfs, &rootDir));
    try(Filesystem_AcquireNodeWithId((FilesystemRef)self->devfs, (InodeId)driverId, &pNode));
    try(Filesystem_Unlink(self->devfs, pNode, rootDir, kUser_Root));

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(rootDir);

    return err;
}

DriverId DriverCatalog_GetDriverIdForName(DriverCatalogRef _Nonnull self, const char* _Nonnull name)
{
    decl_try_err();
    DriverId r = kDriverId_None;
    InodeRef rootDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    pc.name = name;
    pc.count = String_Length(name);

    try(Filesystem_AcquireRootDirectory(self->devfs, &rootDir));
    try(Filesystem_AcquireNodeForName(self->devfs, rootDir, &pc, kUser_Root, NULL, &pNode));
    r = Inode_GetId(pNode);

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(rootDir);

    return r;
}

void DriverCatalog_CopyNameForDriverId(DriverCatalogRef _Nonnull self, DriverId driverId, char* buf, size_t bufSize)
{
    decl_try_err();
    InodeRef rootDir = NULL;
    InodeRef pNode = NULL;
    MutablePathComponent pc;

    if (bufSize < 1) {
        return;
    }

    buf[0] = '\0';
    pc.name = buf;
    pc.capacity = bufSize;
    pc.count = 0;

    try(Filesystem_AcquireRootDirectory(self->devfs, &rootDir));
    try(Filesystem_GetNameOfNode(self->devfs, rootDir, driverId, kUser_Root, &pc));

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(rootDir);
}

DriverRef _Nullable DriverCatalog_CopyDriverForName(DriverCatalogRef _Nonnull self, const char* _Nonnull name)
{
    decl_try_err();
    DriverRef r = NULL;
    InodeRef rootDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    pc.name = name;
    pc.count = String_Length(name);

    try(Filesystem_AcquireRootDirectory(self->devfs, &rootDir));
    try(Filesystem_AcquireNodeForName(self->devfs, rootDir, &pc, kUser_Root, NULL, &pNode));
    r = DevFS_CopyDriverForNode(self->devfs, pNode);

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(rootDir);

    return r;
}

DriverRef _Nullable DriverCatalog_CopyDriverForDriverId(DriverCatalogRef _Nonnull self, DriverId driverId)
{
    decl_try_err();
    DriverRef r = NULL;
    InodeRef pNode = NULL;

    try(Filesystem_AcquireNodeWithId((FilesystemRef)self->devfs, (InodeId)driverId, &pNode));
    r = DevFS_CopyDriverForNode(self->devfs, pNode);

catch:
    Inode_Relinquish(pNode);

    return r;
}
