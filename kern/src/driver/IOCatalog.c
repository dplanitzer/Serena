//
//  IOCatalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//


#include "IOCatalog.h"
#include <string.h>
#include <driver/Driver.h>
#include <filemanager/FileHierarchy.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/kernfs/KernFS.h>
#include <filesystem/kernfs/KfsSpecial.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


typedef struct IOCatalog {
    mtx_t                       mtx;
    FilesystemRef _Nonnull      fs;
    FileHierarchyRef _Nonnull   fh;
    InodeRef _Nonnull           rootDirectory;
} IOCatalog;


IOCatalogRef gIOCatalog;


errno_t IOCatalog_Create(IOCatalogRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    IOCatalogRef self;

    try(kalloc_cleared(sizeof(IOCatalog), (void**) &self));
    
    try(KernFS_Create(FS_CATALOG_DEV, (KernFSRef*)&self->fs));
    try(FilesystemManager_RegisterFilesystem(gFilesystemManager, self->fs));
    try(Filesystem_Start(self->fs, ""));
    try(FileHierarchy_Create(self->fs, &self->fh));
    try(Filesystem_AcquireRootDirectory(self->fs, &self->rootDirectory));
    mtx_init(&self->mtx);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

FilesystemRef _Nonnull IOCatalog_GetFilesystem(IOCatalogRef _Nonnull self)
{
    return self->fs;
}

errno_t IOCatalog_AcquireNodeForPath(IOCatalogRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp)
{
    return FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, UID_ROOT, GID_ROOT, rp);
}

errno_t IOCatalog_AcquireNodeForDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull driver, InodeRef _Nullable * _Nonnull pOutNode)
{
    return Filesystem_AcquireNodeWithId(self->fs, (ino_t)driver->pid, pOutNode);
}

errno_t IOCatalog_Open(IOCatalogRef _Nonnull self, const char* _Nonnull path, fd_flags_t oflags, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    ResolvedPath rp;

    err = FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, UID_ROOT, GID_ROOT, &rp);
    if (err == EOK) {
        Inode_Lock(rp.inode);
        if (!Inode_IsDirectory(rp.inode)) {
            err = Inode_CreateHandler(rp.inode, oflags, pOutHandler);
        }
        else {
            err = EISDIR;
        }
        Inode_Unlock(rp.inode);
    }

    ResolvedPath_Deinit(&rp);
    return err;
}

static errno_t _acquire_folder(IOCatalogRef _Nonnull self, CatalogId folderId, InodeRef _Nullable * _Nonnull pOutDir)
{
    if (folderId == kCatalogId_None) {
        return Filesystem_AcquireRootDirectory(self->fs, pOutDir);
    }
    else {
        return Filesystem_AcquireNodeWithId(self->fs, (ino_t)folderId, pOutDir);
    }
}

// Publishes a folder with the name 'name' to the catalog. Pass kIOCatalog_None as
// the 'parentFolderId' to create the new folder inside the root folder. 
errno_t IOCatalog_PublishFolder(IOCatalogRef _Nonnull self, CatalogId parentFolderId, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutFolderId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutFolderId = kCatalogId_None;

    pc.name = be->name;
    pc.count = strlen(be->name);

    err = _acquire_folder(self, parentFolderId, &pDir);
    if (err == EOK) {
        err = Filesystem_CreateNode(self->fs, pDir, &pc, NULL, be->uid, be->gid, FS_FTYPE_DIR, be->perms, &pNode);
        if (err == EOK) {
            *pOutFolderId = (CatalogId)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}


errno_t IOCatalog_Unpublish(IOCatalogRef _Nonnull self, CatalogId folderId, CatalogId entryId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;

    if (folderId == kCatalogId_None && entryId == kCatalogId_None) {
        return EOK;
    }
    
    // Get the bus directory or devfs root
    err = _acquire_folder(self, folderId, &pDir);
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


errno_t IOCatalog_PublishDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull drv, CatalogId folderId, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId)
{
    CatalogEntry ce;

    ce.name = de->name;
    ce.resource = (ObjectRef)drv;
    ce.func = de->func;
    ce.perms = de->perms;
    ce.uid = de->uid;
    ce.gid = de->gid;

    return IOCatalog_PublishEntry(self, folderId, &ce, pOutId);
}

errno_t IOCatalog_PublishEntry(IOCatalogRef _Nonnull self, CatalogId folderId, const CatalogEntry* _Nonnull ce, did_t* _Nullable pOutId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;
    KfsHandlerNodeArgs args;

    *pOutId = kCatalogId_None;

    pc.name = ce->name;
    pc.count = strlen(ce->name);

    args.resource = ce->resource;
    args.func = ce->func;
    args.perms = ce->perms;
    args.uid = ce->uid;
    args.gid = ce->gid;

    err = _acquire_folder(self, folderId, &pDir);
    if (err == EOK) {
        err = KernFS_CreateHandlerNode((KernFSRef)self->fs, pDir, &pc, &args, &pNode);
        if (err == EOK) {
            *pOutId = (did_t)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}
