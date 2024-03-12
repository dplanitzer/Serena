//
//  diskimage.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <SerenaFS.h>


static void failed(errno_t err)
{
    printf("Error: %d\n", err);
    exit(EXIT_FAILURE);
}

static errno_t formatDiskImage(DiskDriverRef _Nonnull pDisk)
{
    User user = {0, 0};
    const FilePermissions ownerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions otherPerms = kFilePermission_Read | kFilePermission_Execute;
    const FilePermissions dirPerms = FilePermissions_Make(ownerPerms, otherPerms, otherPerms);

    return SerenaFS_FormatDrive(pDisk, user, dirPerms);
}

static void createDiskImage(const char* pRootPath, const char* pDstPath)
{
    decl_try_err();
    DiskDriverRef pDisk = NULL;
    SerenaFSRef pFS = NULL;

    DiskDriver_Create(512, 128, &pDisk);
    
    try(formatDiskImage(pDisk));

    try(SerenaFS_Create(&pFS));
    // XXX iterate the host directory 'pRootPath' and create the file and directories
    // XXX which we encounter in our SeFS disk

    try(DiskDriver_WriteToPath(pDisk, pDstPath));
    
    SerenaFS_Destroy(pFS);
    DiskDriver_Destroy(pDisk);
    printf("Done\n");
    return;

catch:
    failed(err);
}


////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    if (argc > 1) {
        if (!strcmp(argv[1], "create")) {
            if (argc > 3) {
                createDiskImage(argv[2], argv[3]);
                return EXIT_SUCCESS;
            }
        }
    }

    printf("diskimage <action> ...\n");
    printf("   create <root_path> <dimg_path> ...   Creates a SerenaFS formatted disk image file 'dimg_path' which stores a recursive copy of the directory hierarchy and files located at 'root_path'\n");

    return EXIT_FAILURE;
}
