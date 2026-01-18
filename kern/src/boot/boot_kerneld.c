//
//  boot_kerneld.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <filemanager/FileManager.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/kernfs/KernFS.h>
#include <kpi/mount.h>
#include <process/ProcessPriv.h>
#include <process/ProcessManager.h>
#include <Catalog.h>


errno_t kerneld_init(void)
{
    decl_try_err();
    KernFSRef kfs = NULL;
    FileHierarchyRef kfh = NULL;

    // Create an empty kfs
    try(KernFS_Create(&kfs));
    try(Filesystem_Start((FilesystemRef)kfs, ""));
    try(FileHierarchy_Create((FilesystemRef)kfs, &kfh));


    // Create kproc
    KernelProcess_Init(kfh, &gKernelProcess);


    // Mount the driver catalog at /dev
    try(FileManager_CreateDirectory(&gKernelProcess->fm, "/dev", 0550));
    try(FileManager_Mount(&gKernelProcess->fm, kMount_Catalog, kCatalogName_Drivers, "/dev", ""));


    // Finally publish kproc
    try(ProcessManager_Publish(gProcessManager, gKernelProcess));

catch:
    return err;
}
