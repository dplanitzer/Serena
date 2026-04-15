//
//  sys_fs.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <filemanager/FilesystemManager.h>
#include <kpi/directory.h>
#include <kpi/disk.h>
#include <kpi/filesystem.h>
#include <process/kio.h>


SYSCALL_4(fs_open, int wd, const char* _Nonnull path, int oflags, int* _Nonnull pOutIoc)
{
    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    return _kopen(vp->proc, pa->path, pa->oflags, pa->pOutIoc);
}

SYSCALL_5(fs_create_file, int wd, const char* _Nonnull path, int oflags, fs_perms_t fsperms, int* _Nonnull pOutIoc)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef chan;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_CreateFile(&pp->fm, pa->path, pa->oflags, pa->fsperms, &chan);
    if (err == EOK) {
        err = IOChannelTable_AdoptChannel(&pp->ioChannelTable, chan, pa->pOutIoc);
    }
    mtx_unlock(&pp->mtx);

    if (err != EOK) {
        IOChannel_Release(chan);
        *(pa->pOutIoc) = -1;
    }
    return err;
}

SYSCALL_3(dir_open, int wd, const char* _Nonnull path, int* _Nonnull pOutIoc)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef chan;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_OpenDirectory(&pp->fm, pa->path, &chan);
    if (err == EOK) {
        err = IOChannelTable_AdoptChannel(&pp->ioChannelTable, chan, pa->pOutIoc);
    }
    mtx_unlock(&pp->mtx);
    
    if (err != EOK) {
        IOChannel_Release(chan);
        *(pa->pOutIoc) = -1;
    }
    return err;
}

SYSCALL_3(fs_create_directory, int wd, const char* _Nonnull path, fs_perms_t fsperms)
{
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    const errno_t err = FileManager_CreateDirectory(&pp->fm, pa->path, pa->fsperms);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_3(fs_attr, int wd, const char* _Nonnull path, fs_attr_t* _Nonnull attr)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_GetAttributes(&pp->fm, pa->path, pa->attr);
    mtx_unlock(&pp->mtx);    
    return err;
}

SYSCALL_3(fs_truncate, int wd, const char* _Nonnull path, off_t length)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_TruncateFile(&pp->fm, pa->path, pa->length);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_3(fs_access, int wd, const char* _Nonnull path, int mode)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_CheckAccess(&pp->fm, pa->path, pa->mode);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_3(fs_unlink, int wd, const char* _Nonnull path, int mode)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_Unlink(&pp->fm, pa->path, pa->mode);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_4(fs_rename, int owd, const char* _Nonnull oldPath, int nwd,  const char* _Nonnull newPath)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->owd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }
    if (pa->nwd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_Rename(&pp->fm, pa->oldPath, pa->newPath);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_4(fs_setowner, int wd, const char* _Nonnull path, uid_t uid, gid_t gid)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_SetFileOwner(&pp->fm, pa->path, pa->uid, pa->gid);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_3(fs_setperms, int wd, const char* _Nonnull path, fs_perms_t fsperms)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_SetFilePermissions(&pp->fm, pa->path, pa->fsperms);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_3(fs_settimes, int wd, const char* _Nonnull path, const nanotime_t times[_Nullable 2])
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->wd != FD_CWD) {
        //XXX not yet
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    err = FileManager_SetFileTimestamps(&pp->fm, pa->path, pa->times);
    mtx_unlock(&pp->mtx);
    return err;
}

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

SYSCALL_4(fs_property, fsid_t fsid, int flavor, char* _Nonnull buf, size_t bufSize)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);

    switch (pa->flavor) {
        case FS_PROP_LABEL: {
            FilesystemRef fsp = FilesystemManager_CopyFilesystemForId(gFilesystemManager, pa->fsid);

            if (fsp) {
                err = Filesystem_GetLabel(fsp, pa->buf, pa->bufSize);
                Object_Release(fsp);
            }
            else {
                err = ESRCH;
            }
            break;
        }

        case FS_PROP_DISKPATH:
            err = FileManager_GetFilesystemDiskPath(&pp->fm, pa->fsid, pa->buf, pa->bufSize);
            break;

        default:
            err = EINVAL;
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
