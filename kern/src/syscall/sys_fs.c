//
//  sys_fs.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <filemanager/FilesystemManager.h>
#include <kpi/disk.h>
#include <kpi/fs.h>


SYSCALL_3(fs_info, fsid_t fsid, int flavor, fs_info_ref _Nonnull info)
{
    decl_try_err();
    FilesystemRef fsp = FilesystemManager_CopyFilesystemForId(gFilesystemManager, pa->fsid);

    if (fsp) {
        err = Filesystem_GetInfo(fsp, pa->flavor, pa->info);
        Object_Release(fsp);
    }
    else {
        err = ESRCH;
    }

    return err;
}

SYSCALL_3(fs_diskpath, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_GetFilesystemDiskPath(&pp->fm, pa->fsid, pa->buf, pa->bufSize);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_3(fs_label, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    FilesystemRef fsp = FilesystemManager_CopyFilesystemForId(gFilesystemManager, pa->fsid);

    if (fsp) {
        err = Filesystem_GetLabel(fsp, pa->buf, pa->bufSize);
        Object_Release(fsp);
    }
    else {
        err = ESRCH;
    }
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(fs_setlabel, fsid_t fsid, const char* _Nonnull label)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    FilesystemRef fsp = FilesystemManager_CopyFilesystemForId(gFilesystemManager, pa->fsid);

    if (fsp) {
        err = Filesystem_SetLabel(fsp, pa->label);
        Object_Release(fsp);
    }
    else {
        err = ESRCH;
    }
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_4(fs_mount, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_Mount(&pp->fm, pa->objectType, pa->objectName, pa->atDirPath, pa->params);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(fs_unmount, const char* _Nonnull atDirPath, int flags)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_Unmount(&pp->fm, pa->atDirPath, pa->flags);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_0(fs_sync)
{
    FilesystemManager_Sync(gFilesystemManager);
    return EOK;
}
