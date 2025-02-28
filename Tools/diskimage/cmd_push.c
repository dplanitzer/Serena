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


static errno_t _create_file(FileManagerRef _Nonnull fm, const char* _Nonnull path, FilePermissions perms, uid_t uid, gid_t gid, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef chan = NULL;

    err = FileManager_OpenFile(fm, path, kOpen_Write | kOpen_Truncate, &chan);
    if (err == ENOENT) {
        // File doesn't exist yet. Create it
        err = FileManager_CreateFile(fm, path, kOpen_Write, perms, &chan);
    }

    if (err == EOK) {
        // Update the inode metadata
        MutableFileInfo info;

        info.modify = kModifyFileInfo_Permissions | kModifyFileInfo_UserId | kModifyFileInfo_GroupId;
        info.uid = uid;
        info.gid = gid;
        info.permissions = perms;
        info.permissionsModifyMask = 0xffff;
        err = FileManager_SetFileInfo_ioc(fm, chan, &info);

        if (err != EOK) {
            FileManager_Unlink(fm, path);
        }
    }

    *pOutChannel = chan;
    return err;
}

errno_t cmd_push(FilePermissions filePerms, uid_t uid, gid_t gid, const char* _Nonnull srcPath, const char* _Nonnull path, const char* _Nonnull dmgPath)
{
    decl_try_err();
    DiskControllerRef self;
    IOChannelRef chan = NULL;
    FILE* fp = NULL;
    char* buf = NULL;
    char* dstPath = NULL;

    try(DiskController_CreateWithContentsOfPath(dmgPath, &self));
    try_null(buf, malloc(BLOCK_SIZE), ENOMEM);
    try_null(dstPath, create_dst_path(srcPath, path), ENOMEM);

    try(_create_file(&self->fm, dstPath, filePerms, uid, gid, &chan));
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
    free(dstPath);
    free(buf);
    DiskController_Destroy(self);
    return err;
}
