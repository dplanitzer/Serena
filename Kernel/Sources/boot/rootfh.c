//
//  rootfh.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <klib/klib.h>
#include <driver/DriverCatalog.h>
#include <filemanager/FileHierarchy.h>

extern FilesystemRef _Nullable create_boot_filesystem(void);


// Creates the root file hierarchy based on the detected boot filesystem. Halts
// the machine if anything goes wrong.
FileHierarchyRef _Nonnull create_root_file_hierarchy(void)
{
    decl_try_err();

    FilesystemRef bootfs = create_boot_filesystem();
    if (bootfs == NULL) {
        print("No boot device found.\nHalting...\n");
        while(true);
        /* NOT REACHED */
    }

    FileHierarchyRef fh = NULL;
    InodeRef rootDir = NULL;
    ResolvedPath rp;

    try(FileHierarchy_Create(bootfs, &fh));
    rootDir = FileHierarchy_AcquireRootDirectory(fh);
    err = FileHierarchy_AcquireNodeForPath(fh, kPathResolution_Target, "/System/Devices", rootDir, rootDir, kUser_Root, &rp);
    if (err == EOK) {
        try(FileHierarchy_AttachFilesystem(fh, (FilesystemRef)DriverCatalog_GetDevicesFilesystem(gDriverCatalog), rp.inode));
    }
    ResolvedPath_Deinit(&rp);
    Inode_Relinquish(rootDir);

    return fh;

catch:
    print("Unable to boot (%d).\nHalting...\n", err);
    while(true);
    /* NOT REACHED */
}
