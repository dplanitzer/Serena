//
//  FSCatalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//


#include "FSCatalog.h"
#include <filemanager/FileHierarchy.h>
#include <filesystem/kernfs/KernFS.h>


typedef struct FSCatalog {
    KernFSRef _Nonnull          fs;
    FileHierarchyRef _Nonnull   fh;
    InodeRef _Nonnull           rootDirectory;
} FSCatalog;


FSCatalogRef _Nonnull  gFSCatalog;

errno_t FSCatalog_Create(FSCatalogRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FSCatalogRef self;
    
    try(kalloc_cleared(sizeof(FSCatalog), (void**) &self));
    
    try(KernFS_Create(&self->fs));
    try(Filesystem_Start((FilesystemRef)self->fs, NULL, 0));
    try(FileHierarchy_Create((FilesystemRef)self->fs, &self->fh));
    try(Filesystem_AcquireRootDirectory((FilesystemRef)self->fs, &self->rootDirectory));

    *pOutSelf = self;
    return EOK;

catch:
    FSCatalog_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void FSCatalog_Destroy(FSCatalogRef _Nullable self)
{
    if (self) {
        Inode_Relinquish(self->rootDirectory);
        self->rootDirectory = NULL;

        Object_Release(self->fh);
        self->fh = NULL;

        Filesystem_Stop((FilesystemRef)self->fs);
        Object_Release(self->fs);
        self->fs = NULL;

        kfree(self);
    }
}

FilesystemRef _Nonnull FSCatalog_CopyFilesystem(FSCatalogRef _Nonnull self)
{
    return Object_RetainAs(self->fs, Filesystem);
}

errno_t FSCatalog_Publish(FSCatalogRef _Nonnull self, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, FilesystemRef _Nonnull fs, FSCatalogId* _Nonnull pOutFSCatalogId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;

    *pOutFSCatalogId = kFSCatalogId_None;

    pc.name = name;
    pc.count = String_Length(name);

    err = Filesystem_AcquireRootDirectory((FilesystemRef)self->fs, &pDir);
    if (err == EOK) {
        err = KernFS_CreateFilesystem(self->fs, pDir, &pc, fs, uid, gid, perms, &pNode);
        if (err == EOK) {
            *pOutFSCatalogId = (FSCatalogId)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}

errno_t FSCatalog_Unpublish(FSCatalogRef _Nonnull self, FSCatalogId fsCatalogId)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;

    if (fsCatalogId == kFSCatalogId_None) {
        return EOK;
    }
    
    // Get the bus directory or devfs root
    err = Filesystem_AcquireRootDirectory((FilesystemRef)self->fs, &pDir);
    if (err == EOK) {
        err = Filesystem_AcquireNodeWithId((FilesystemRef)self->fs, (ino_t)fsCatalogId, &pNode);
        if (err == EOK) {
            err = Filesystem_Unlink(self->fs, pNode, pDir);
        }
    }

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}

errno_t FSCatalog_OpenFilesystem(FSCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath rp;

    try(FileHierarchy_AcquireNodeForPath(self->fh, kPathResolution_Target, path, self->rootDirectory, self->rootDirectory, kUserId_Root, kGroupId_Root, &rp));
    err = Inode_CreateChannel(rp.inode, mode, pOutChannel);

catch:
    ResolvedPath_Deinit(&rp);
    return err;
}
