//
//  Catalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//


#include "Catalog.h"
#include <filemanager/FileHierarchy.h>
#include <filesystem/kernfs/KernFS.h>


typedef struct Catalog {
    FilesystemRef _Nonnull      fs;
    FileHierarchyRef _Nonnull   fh;
    InodeRef _Nonnull           rootDirectory;
} Catalog;


CatalogRef _Nonnull  gDriverCatalog;
CatalogRef _Nonnull  gFSCatalog;


errno_t Catalog_Create(const char* _Nonnull name, CatalogRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CatalogRef self;
    
    try(kalloc_cleared(sizeof(Catalog), (void**) &self));
    
    try(KernFS_Create(name, (KernFSRef*)&self->fs));
    try(Filesystem_Start(self->fs, NULL, 0));
    try(FileHierarchy_Create(self->fs, &self->fh));
    try(Filesystem_AcquireRootDirectory(self->fs, &self->rootDirectory));

    *pOutSelf = self;
    return EOK;

catch:
    Catalog_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void Catalog_Destroy(CatalogRef _Nullable self)
{
    if (self) {
        Inode_Relinquish(self->rootDirectory);
        self->rootDirectory = NULL;

        Object_Release(self->fh);
        self->fh = NULL;

        Filesystem_Stop(self->fs);
        Object_Release(self->fs);
        self->fs = NULL;

        kfree(self);
    }
}

FilesystemRef _Nonnull Catalog_CopyFilesystem(CatalogRef _Nonnull self)
{
    return Object_RetainAs(self->fs, Filesystem);
}

errno_t Catalog_IsPublished(CatalogRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    ResolvedPath rp;

    err = FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, kUserId_Root, kGroupId_Root, &rp);
    ResolvedPath_Deinit(&rp);

    return err;
}

errno_t Catalog_Open(CatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath rp;

    try(FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, kUserId_Root, kGroupId_Root, &rp));
    err = Inode_CreateChannel(rp.inode, mode, pOutChannel);

catch:
    ResolvedPath_Deinit(&rp);
    return err;
}

errno_t Catalog_GetPath(CatalogRef _Nonnull self, CatalogId cid, size_t bufSize, char* _Nonnull buf)
{
    decl_try_err();
    InodeRef nd;

    if (bufSize < 1) {
        return EINVAL;
    }

    err = Filesystem_AcquireNodeWithId(self->fs, cid, &nd);
    if (err == EOK) {
        err = FileHierarchy_GetPath(self->fh, nd, self->rootDirectory, kUserId_Root, kGroupId_Root, buf, bufSize);
        Inode_Relinquish(nd);
    }
    else {
        *buf = '\0';
    }

    return err;
}


static errno_t _Catalog_AcquireFolder(CatalogRef _Nonnull self, CatalogId folderId, InodeRef _Nullable * _Nonnull pOutDir)
{
    if (folderId == kCatalogId_None) {
        return Filesystem_AcquireRootDirectory(self->fs, pOutDir);
    }
    else {
        return Filesystem_AcquireNodeWithId(self->fs, (ino_t)folderId, pOutDir);
    }
}

// Publishes a folder with the name 'name' to the catalog. Pass kCatalog_None as
// the 'parentFolderId' to create the new folder inside the root folder. 
errno_t Catalog_PublishFolder(CatalogRef _Nonnull self, CatalogId parentFolderId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, CatalogId* _Nonnull pOutFolderId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutFolderId = kCatalogId_None;

    pc.name = name;
    pc.count = String_Length(name);

    err = _Catalog_AcquireFolder(self, parentFolderId, &pDir);
    if (err == EOK) {
        err = Filesystem_CreateNode(self->fs, kFileType_Directory, pDir, &pc, NULL, uid, gid, perms, &pNode);
        if (err == EOK) {
            *pOutFolderId = (CatalogId)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}


errno_t Catalog_Unpublish(CatalogRef _Nonnull self, CatalogId folderId, CatalogId entryId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;

    if (folderId == kCatalogId_None && entryId == kCatalogId_None) {
        return EOK;
    }
    
    // Get the bus directory or devfs root
    err = _Catalog_AcquireFolder(self, folderId, &pDir);
    if (err == EOK) {
        // Get the parent of the directory or the driver entry
        if (entryId == kCatalogId_None) {
            pNode = pDir;
            pDir = NULL;

            err = Filesystem_AcquireParentNode(self->fs, pDir, &pDir);
        }
        else {
            err = Filesystem_AcquireNodeWithId(self->fs, (ino_t)entryId, &pNode);
        }


        // Delete the directory or the driver entry
        if (err == EOK) {
            err = Filesystem_Unlink(self->fs, pNode, pDir);
        }
    }

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}


errno_t Catalog_PublishDriver(CatalogRef _Nonnull self, CatalogId folderId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, DriverRef _Nonnull driver, intptr_t arg, CatalogId* _Nonnull pOutCatalogId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutCatalogId = kCatalogId_None;

    pc.name = name;
    pc.count = String_Length(name);

    err = _Catalog_AcquireFolder(self, folderId, &pDir);
    if (err == EOK) {
        err = KernFS_CreateDevice((KernFSRef)self->fs, pDir, &pc, driver, arg, uid, gid, perms, &pNode);
        if (err == EOK) {
            *pOutCatalogId = (CatalogId)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}

errno_t Catalog_PublishFilesystem(CatalogRef _Nonnull self, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, FilesystemRef _Nonnull fs, CatalogId* _Nonnull pOutCatalogId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutCatalogId = kCatalogId_None;

    pc.name = name;
    pc.count = String_Length(name);

    err = Filesystem_AcquireRootDirectory(self->fs, &pDir);
    if (err == EOK) {
        err = KernFS_CreateFilesystem((KernFSRef)self->fs, pDir, &pc, fs, uid, gid, perms, &pNode);
        if (err == EOK) {
            *pOutCatalogId = (CatalogId)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}
