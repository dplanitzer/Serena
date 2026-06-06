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


struct matcher {
    queue_node_t        qe;
    drv_match_func_t    func;
    void* _Nullable     arg;
    iocat_t             cats[IOCAT_MAX];
};
typedef struct matcher* matcher_t;


typedef struct Catalog {
    FilesystemRef _Nonnull      fs;
    FileHierarchyRef _Nonnull   fh;
    InodeRef _Nonnull           rootDirectory;
    mtx_t                       matchersLock;
    queue_t/*<matcher_t*/       matchers;
} Catalog;


CatalogRef gIOCatalog;


errno_t IOCatalog_Create(CatalogRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CatalogRef self;

    try(kalloc_cleared(sizeof(Catalog), (void**) &self));
    
    try(KernFS_Create(FS_CATALOG_DEV, (KernFSRef*)&self->fs));
    try(FilesystemManager_RegisterFilesystem(gFilesystemManager, self->fs));
    try(Filesystem_Start(self->fs, ""));
    try(FileHierarchy_Create(self->fs, &self->fh));
    try(Filesystem_AcquireRootDirectory(self->fs, &self->rootDirectory));
    mtx_init(&self->matchersLock);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

FilesystemRef _Nonnull IOCatalog_GetFilesystem(CatalogRef _Nonnull self)
{
    return self->fs;
}

errno_t IOCatalog_IsPublished(CatalogRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    ResolvedPath rp;

    err = FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, UID_ROOT, GID_ROOT, &rp);
    ResolvedPath_Deinit(&rp);

    return err;
}

errno_t IOCatalog_AcquireNodeForPath(CatalogRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp)
{
    return FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, UID_ROOT, GID_ROOT, rp);
}

errno_t IOCatalog_Open(CatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath rp;

    err = FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, UID_ROOT, GID_ROOT, &rp);
    if (err == EOK) {
        Inode_Lock(rp.inode);
        if (!Inode_IsDirectory(rp.inode)) {
            err = Inode_CreateChannel(rp.inode, mode, pOutChannel);
        }
        else {
            err = EISDIR;
        }
        Inode_Unlock(rp.inode);
    }

    ResolvedPath_Deinit(&rp);
    return err;
}

static errno_t _IOCatalog_AcquireFolder(CatalogRef _Nonnull self, CatalogId folderId, InodeRef _Nullable * _Nonnull pOutDir)
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
errno_t IOCatalog_PublishFolder(CatalogRef _Nonnull self, CatalogId parentFolderId, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutFolderId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutFolderId = kCatalogId_None;

    pc.name = be->name;
    pc.count = strlen(be->name);

    err = _IOCatalog_AcquireFolder(self, parentFolderId, &pDir);
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


errno_t IOCatalog_Unpublish(CatalogRef _Nonnull self, CatalogId folderId, CatalogId entryId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;

    if (folderId == kCatalogId_None && entryId == kCatalogId_None) {
        return EOK;
    }
    
    // Get the bus directory or devfs root
    err = _IOCatalog_AcquireFolder(self, folderId, &pDir);
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


errno_t IOCatalog_PublishDriver(CatalogRef _Nonnull self, DriverRef _Nonnull drv, CatalogId folderId, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutId = kCatalogId_None;

    pc.name = de->name;
    pc.count = strlen(de->name);

    err = _IOCatalog_AcquireFolder(self, folderId, &pDir);
    if (err == EOK) {
        err = KernFS_CreateDriverNode((KernFSRef)self->fs, pDir, &pc, drv, de->arg, de->uid, de->gid, de->perms, &pNode);
        if (err == EOK) {
            *pOutId = (did_t)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}


errno_t IOCatalog_CopyDriverForId(CatalogRef _Nonnull self, CatalogId id, DriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    InodeRef ip = NULL;
    DriverRef driver = NULL;

    try(Filesystem_AcquireNodeWithId(self->fs, (ino_t)id, &ip));
    Inode_Lock(ip);
    if (Inode_GetFileType(ip) == FS_FTYPE_DEV) {
        driver = Object_RetainAs(((KfsSpecialRef)ip)->instance, Driver);
    }
    Inode_Unlock(ip);

catch:
    Inode_Relinquish(ip);
    *pOutDriver = driver;

    return err;
}

errno_t IOCatalog_CopyMatchingDrivers(CatalogRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nullable * _Nonnull pOutDrivers)
{
    return KernFS_CopyMatchingDrivers((KernFSRef)self->fs, cats, pOutDrivers);
}

errno_t IOCatalog_StartMatching(CatalogRef _Nonnull self, const iocat_t* _Nonnull cats, drv_match_func_t _Nonnull f, void* _Nullable arg)
{
    decl_try_err();
    DriverRef* drivers = NULL;
    matcher_t pm = NULL;
    int i = 0;

    mtx_lock(&self->matchersLock);

    // Create a matcher entry
    try(kalloc_cleared(sizeof(struct matcher), (void**)&pm));
    pm->func = f;
    pm->arg = arg;
    while (i < IOCAT_MAX-1 && cats[i] != IOCAT_END) {
        pm->cats[i] = cats[i];
        i++;
    }
    pm->cats[i] = IOCAT_END;
    queue_add_last(&self->matchers, &pm->qe);


    // Tell the matcher about all existing drivers
    try(KernFS_CopyMatchingDrivers((KernFSRef)self->fs, cats, &drivers));
    i = 0;
    while (drivers[i]) {
        f(arg, drivers[i], IONOTIFY_STARTED);
        i++;
    }
    kfree(drivers);

catch:
    mtx_unlock(&self->matchersLock);

    return err;
}

void IOCatalog_StopMatching(CatalogRef _Nonnull self, drv_match_func_t _Nonnull f, void* _Nullable arg)
{
    matcher_t pprev = NULL;

    mtx_lock(&self->matchersLock);
    queue_for_each(&self->matchers, struct matcher, it,
        if (it->func == f && it->arg == arg) {
            queue_remove(&self->matchers, &pprev->qe, &it->qe);
            break;
        }
        pprev = it;
    )
    mtx_unlock(&self->matchersLock);
}

static void _do_match_callouts(CatalogRef _Nonnull self, DriverRef _Nonnull driver, int notify)
{
    mtx_lock(&self->matchersLock);
    queue_for_each(&self->matchers, struct matcher, it,
        if (Driver_HasSomeCategories((DriverRef)driver, it->cats)) {
            it->func(it->arg, driver, notify);
        }
    )
    mtx_unlock(&self->matchersLock);
}

void IOCatalog_OnDriverStarted(CatalogRef _Nonnull self, DriverRef _Nonnull driver)
{
    _do_match_callouts(self, driver, IONOTIFY_STARTED);
}

void IOCatalog_OnDriverStopping(CatalogRef _Nonnull self, DriverRef _Nonnull driver)
{
    _do_match_callouts(self, driver, IONOTIFY_STOPPING);
}
