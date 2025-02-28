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

FilesystemRef _Nullable DriverCatalog_CopyChapterFilesystem(DriverCatalogRef _Nonnull self, const char* _Nonnull chapterName)
{
    if (String_Equals(chapterName, "topology")) {
        return Object_RetainAs(self->devfs, Filesystem);
    }
    else {
        return NULL;
    }
}

static errno_t DriverCatalog_AcquireBusDirectory(DriverCatalogRef _Nonnull self, DriverCatalogId busCatalogId, InodeRef _Nullable * _Nonnull pOutDir)
{
    if (busCatalogId == kDriverCatalogId_None) {
        return Filesystem_AcquireRootDirectory(self->devfs, pOutDir);
    }
    else {
        return Filesystem_AcquireNodeWithId((FilesystemRef)self->devfs, (ino_t)busCatalogId, pOutDir);
    }
}

errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, DriverCatalogId busCatalogId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, DriverRef _Nonnull driver, intptr_t arg, DriverCatalogId* _Nonnull pOutDriverCatalogId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutDriverCatalogId = kDriverCatalogId_None;

    pc.name = name;
    pc.count = String_Length(name);

    err = DriverCatalog_AcquireBusDirectory(self, busCatalogId, &pDir);
    if (err == EOK) {
        err = DevFS_CreateDevice(self->devfs, pDir, &pc, driver, arg, uid, gid, perms, &pNode);
        if (err == EOK) {
            *pOutDriverCatalogId = (DriverCatalogId)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}

// Publishes a bus directory with the name 'name' to the driver catalog.
errno_t DriverCatalog_PublishBus(DriverCatalogRef _Nonnull self, DriverCatalogId parentBusId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, DriverCatalogId* _Nonnull pOutBusCatalogId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutBusCatalogId = kDriverCatalogId_None;

    pc.name = name;
    pc.count = String_Length(name);

    err = DriverCatalog_AcquireBusDirectory(self, parentBusId, &pDir);
    if (err == EOK) {
        err = Filesystem_CreateNode((FilesystemRef)self->devfs, kFileType_Directory, pDir, &pc, NULL, uid, gid, perms, &pNode);
        if (err == EOK) {
            *pOutBusCatalogId = (DriverCatalogId)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}

errno_t DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverCatalogId busCatalogId, DriverCatalogId driverCatalogId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;

    if (busCatalogId == kDriverCatalogId_None && driverCatalogId == kDriverCatalogId_None) {
        return EOK;
    }
    
    // Get the bus directory or devfs root
    err = DriverCatalog_AcquireBusDirectory(self, busCatalogId, &pDir);
    if (err == EOK) {
        // Get the parent of the bus directory or the driver entry
        if (driverCatalogId == kDriverCatalogId_None) {
            pNode = pDir;
            pDir = NULL;

            err = Filesystem_AcquireNodeForName(self->devfs, pDir, &kPathComponent_Parent, kUserId_Root, kGroupId_Root, NULL, &pDir);
        }
        else {
            err = Filesystem_AcquireNodeWithId((FilesystemRef)self->devfs, (ino_t)driverCatalogId, &pNode);
        }


        // Delete the bus directory or the driver entry
        if (err == EOK) {
            err = Filesystem_Unlink(self->devfs, pNode, pDir);
        }
    }

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}

errno_t DriverCatalog_IsDriverPublished(DriverCatalogRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    ResolvedPath rp;

    err = FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, kUserId_Root, kGroupId_Root, &rp);
    ResolvedPath_Deinit(&rp);

    return err;
}

errno_t DriverCatalog_OpenDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath rp;

    try(FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, kUserId_Root, kGroupId_Root, &rp));
    err = Inode_CreateChannel(rp.inode, mode, pOutChannel);

catch:
    ResolvedPath_Deinit(&rp);
    return err;
}
