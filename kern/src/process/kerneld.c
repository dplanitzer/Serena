//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "kerneld.h"
#include "ProcessPriv.h"
#include <filemanager/FileHierarchy.h>
#include <filemanager/FileManager.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/kernfs/KernFS.h>
#include <kpi/filesystem.h>
#include <process/ProcessManager.h>


static const char*      g_systemd_argv[2] = { "/System/Commands/systemd", NULL };
static proc_spawnattr_t g_systemd_spawn;
static const char*      g_kernel_argv[2] = { "kerneld", NULL };
static const char*      g_kernel_env[1] = { NULL };
static proc_ctx_t       g_kernel_ctx;
static struct Process   g_kernel_proc_storage;
ProcessRef _Nonnull     gKernelProcess;


errno_t kerneld_init(void)
{
    decl_try_err();
    KernFSRef kfs = NULL;
    FileHierarchyRef kfh = NULL;

    // Create an empty kfs
    try(KernFS_Create(&kfs));
    try(Filesystem_Start((FilesystemRef)kfs, ""));
    try(FileHierarchy_Create((FilesystemRef)kfs, &kfh));


    InodeRef rootDir = FileHierarchy_AcquireRootDirectory(kfh);
    
    Process_Init(&g_kernel_proc_storage, NULL, kfh, rootDir, rootDir);
    Inode_Relinquish(rootDir);

    g_kernel_ctx.argc = 1;
    g_kernel_ctx.argv = g_kernel_argv;
    g_kernel_ctx.envv = g_kernel_env;
    g_kernel_proc_storage.ctx_base= &g_kernel_ctx;

    vcpu_t main_vp = vcpu_current();
    main_vp->proc = &g_kernel_proc_storage;
    main_vp->id = VCPUID_MAIN;
    main_vp->group_id = VCPUID_MAIN_GROUP;
    deque_add_last(&g_kernel_proc_storage.vcpu_queue, &main_vp->owner_qe);
    g_kernel_proc_storage.vcpu_count++;

    gKernelProcess = &g_kernel_proc_storage;


    // Mount the driver catalog at /dev
    try(FileManager_CreateDirectory(&gKernelProcess->fm, "/dev", 0550));
    try(FileManager_Mount(&gKernelProcess->fm, FS_MOUNT_CATALOG, FS_CATALOG_DRIVERS, "/dev", ""));


    // Finally publish kerneld
    try(ProcessManager_Publish(gProcessManager, gKernelProcess));

catch:
    return err;
}

errno_t kerneld_spawn_systemd(ProcessRef _Nonnull self, FileHierarchyRef _Nonnull fh)
{
    decl_try_err();
    ProcessRef cp = NULL;

    g_systemd_spawn.version = sizeof(struct proc_spawnattr);
    g_systemd_spawn.type = PROC_SPAWN_SESSION_LEADER;
    
    err = Process_CreateChild(self, &g_systemd_spawn, fh, &cp);
    if (err == EOK) {
        ProcessManager_Publish(gProcessManager, cp);

        err = Process_Exec(cp, g_systemd_argv[0], g_systemd_argv, NULL);
    }
    Process_Release(cp);

    return err;
}