//
//  cmd_push.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "DiskController.h"
#include <errno.h>
#include <stdio.h>

#define BLOCK_SIZE  4096

errno_t cmd_push(const char* _Nonnull srcPath, const char* _Nonnull path, const char* _Nonnull dmgPath)
{
    decl_try_err();
    const FilePermissions permissions = FilePermissions_Make(kFilePermission_Read | kFilePermission_Write, kFilePermission_Read | kFilePermission_Write, kFilePermission_Read | kFilePermission_Write);
    DiskControllerRef self;
    IOChannelRef chan = NULL;
    FILE* fp = NULL;
    char* buf = NULL;

    try(DiskController_CreateWithContentsOfPath(dmgPath, &self));
    try_null(buf, malloc(BLOCK_SIZE), ENOMEM);
    
    err = FileManager_OpenFile(&self->fm, path, kOpen_Write | kOpen_Truncate, &chan);
    if (err == EOK) {
        // Update the inode metadata
        MutableFileInfo info;

        info.modify = kModifyFileInfo_Permissions;
        info.permissions = permissions;
        info.permissionsModifyMask = 0xffff;
        err = FileManager_SetFileInfoFromIOChannel(&self->fm, chan, &info);
    }
    else if (err == ENOENT) {
        // File doesn't exist yet. Create it
        err = FileManager_CreateFile(&self->fm, path, kOpen_Write, permissions, &chan);
    }
    throw_iferr(err);

    try_null(fp, fopen(srcPath, "rb"), errno);

    while (true) {
        const size_t nBytesRead = fread(buf, 1, BLOCK_SIZE, fp);
        if (nBytesRead == 0) {
            if (ferror(fp)) {
                err = errno;
            }
            break;
        }

        ssize_t nBytesWritten;
        err = IOChannel_Write(chan, buf, nBytesRead, &nBytesWritten);
        if (err != EOK) {
            break;
        }
    }

    if (err == EOK) {
        // Allow the inode data to get written back to the disk before we save
        // the disk data
        IOChannel_Release(chan);
        chan = NULL;

        err = DiskController_WriteToPath(self, dmgPath);
    }

catch:
    if (fp) {
        fclose(fp);
    }
    IOChannel_Release(chan);
    free(buf);
    DiskController_Destroy(self);
    return err;
}
