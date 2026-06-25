//
//  devfs.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//


#include <kern/devfs.h>
#include <string.h>
#include <filemanager/FileHierarchy.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/kernfs/KernFS.h>
#include <sched/mtx.h>


static mtx_t                       g_mtx;
static FilesystemRef _Nonnull      g_fs;
static FileHierarchyRef _Nonnull   g_fh;
static InodeRef _Nonnull           g_root_dir;


errno_t devfs_init(void)
{
    decl_try_err();

    try(KernFS_Create(FS_CATALOG_DEV, (KernFSRef*)&g_fs));
    try(FilesystemManager_RegisterFilesystem(gFilesystemManager, g_fs));
    try(Filesystem_Start(g_fs, ""));
    try(FileHierarchy_Create(g_fs, &g_fh));
    try(Filesystem_AcquireRootDirectory(g_fs, &g_root_dir));
    mtx_init(&g_mtx);

    return EOK;

catch:
    return err;
}

FilesystemRef _Nonnull devfs_get_fs(void)
{
    return g_fs;
}

errno_t devfs_acquire_node_for_handle(devfs_hnd_t hnd, InodeRef _Nullable * _Nonnull pOutNode)
{
    return (hnd > 0) ? Filesystem_AcquireNodeWithId(g_fs, (ino_t)hnd, pOutNode) : EINVAL;
}

errno_t devfs_add(const devfs_entry_t* _Nonnull ce, devfs_hnd_t* _Nullable pOutHnd)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;
    PathComponent pc;
    KfsHandlerNodeArgs args;

    *pOutHnd = 0;

    pc.name = ce->name;
    pc.count = strlen(ce->name);

    args.resource = ce->resource;
    args.func = ce->func;
    args.perms = ce->perms;
    args.uid = ce->uid;
    args.gid = ce->gid;

    err = Filesystem_AcquireRootDirectory(g_fs, &pDir);
    if (err == EOK) {
        err = KernFS_CreateHandlerNode((KernFSRef)g_fs, pDir, &pc, &args, &pNode);

        if (err == EOK && pOutHnd) {
            *pOutHnd = (devfs_hnd_t)Inode_GetId(pNode);
        }
    }

    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}

errno_t devfs_remove(devfs_hnd_t hnd)
{
    decl_try_err();
    InodeRef pDir = NULL;
    InodeRef pNode = NULL;

    if (hnd == 0) {
        return EOK;
    }
    
    // Get the bus directory or devfs root
    err = Filesystem_AcquireRootDirectory(g_fs, &pDir);
    if (err == EOK) {
        err = Filesystem_AcquireNodeWithId(g_fs, (ino_t)hnd, &pNode);


        // Delete the (driver) entry
        if (err == EOK) {
            err = Filesystem_Unlink(g_fs, pNode, pDir);
        }
    }

catch:
    Inode_Relinquish(pNode);
    Inode_Relinquish(pDir);

    return err;
}
