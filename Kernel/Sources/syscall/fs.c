//
//  fs.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <filemanager/FilesystemManager.h>
#include <time.h>
#include <kpi/disk.h>
#include <kpi/fs.h>


SYSCALL_4(mount, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    Lock_Lock(&pp->lock);
    err = FileManager_Mount(&pp->fm, pa->objectType, pa->objectName, pa->atDirPath, pa->params);
    Lock_Unlock(&pp->lock);
    return err;
}

SYSCALL_2(unmount, const char* _Nonnull atDirPath, UnmountOptions options)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    Lock_Lock(&pp->lock);
    err = FileManager_Unmount(&pp->fm, pa->atDirPath, pa->options);
    Lock_Unlock(&pp->lock);
    return err;
}

SYSCALL_0(sync)
{
    FilesystemManager_Sync(gFilesystemManager);
    return EOK;
}

SYSCALL_3(fsgetdisk, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    Lock_Lock(&pp->lock);
    err = FileManager_GetFilesystemDiskPath(&pp->fm, pa->fsid, pa->buf, pa->bufSize);
    Lock_Unlock(&pp->lock);
    return err;
}
