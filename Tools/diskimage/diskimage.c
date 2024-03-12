//
//  diskimage.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <SerenaFS.h>


void createDiskImage(const char* pRootPath, const char* pDstPath)
{
    DiskDriverRef pDisk = NULL;
    User user = {0, 0};
    const FilePermissions ownerPerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions otherPerms = kFilePermission_Read | kFilePermission_Execute;
    const FilePermissions dirPerms = FilePermissions_Make(ownerPerms, otherPerms, otherPerms);

    DiskDriver_Create(512, 128, &pDisk);
    
    SerenaFS_FormatDrive(pDisk, user, dirPerms);
    DiskDriver_WriteToPath(pDisk, pDstPath);

    DiskDriver_Destroy(pDisk);
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
