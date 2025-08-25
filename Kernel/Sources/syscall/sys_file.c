//
//  sys_file.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <filesystem/IOChannel.h>
#include <time.h>
#include <kern/limits.h>
#include <kern/timespec.h>
#include <kpi/uid.h>
#include <process/kio.h>


SYSCALL_4(mkfile, const char* _Nonnull path, int oflags, mode_t mode, int* _Nonnull pOutIoc)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef chan;

    mtx_lock(&pp->mtx);
    err = FileManager_CreateFile(&pp->fm, pa->path, pa->oflags, pa->mode, &chan);
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

SYSCALL_3(open, const char* _Nonnull path, int oflags, int* _Nonnull pOutIoc)
{
    return _kopen(vp->proc, pa->path, pa->oflags, pa->pOutIoc);
}

SYSCALL_1(close, int fd)
{
    return _kclose(vp->proc, pa->fd);
}

SYSCALL_4(read, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    return _kread(vp->proc, pa->fd, pa->buffer, pa->nBytesToRead, pa->nBytesRead);
}

SYSCALL_4(write, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    return _kwrite(vp->proc, pa->fd, pa->buffer, pa->nBytesToWrite, pa->nBytesWritten);
}

SYSCALL_4(seek, int fd, off_t offset, off_t* _Nullable pOutNewPos, int whence)
{
    return _kseek(vp->proc, pa->fd, pa->offset, pa->pOutNewPos, pa->whence);
}

SYSCALL_4(fcntl, int fd, int cmd, int* _Nonnull pResult, va_list _Nullable ap)
{
    return _kfcntl(vp->proc, pa->fd, pa->cmd, pa->pResult, pa->ap);
}

SYSCALL_3(ioctl, int fd, int cmd, va_list _Nullable ap)
{
    return _kioctl(vp->proc, pa->fd, pa->cmd, pa->ap);
}

SYSCALL_2(opendir, const char* _Nonnull path, int* _Nonnull pOutIoc)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef chan;

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

SYSCALL_2(mkdir, const char* _Nonnull path, mode_t mode)
{
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    const errno_t err = FileManager_CreateDirectory(&pp->fm, pa->path, pa->mode);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(getcwd, char* _Nonnull buffer, size_t bufferSize)
{
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    const errno_t err = FileManager_GetWorkingDirectoryPath(&pp->fm, pa->buffer, pa->bufferSize);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_1(chdir, const char* _Nonnull path)
{
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    const errno_t err = FileManager_SetWorkingDirectoryPath(&pp->fm, pa->path);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(stat, const char* _Nonnull path, struct stat* _Nonnull pOutInfo)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_GetFileInfo(&pp->fm, pa->path, pa->pOutInfo);
    mtx_unlock(&pp->mtx);    
    return err;
}

SYSCALL_2(fstat, int fd, struct stat* _Nonnull pOutInfo)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = FileManager_GetFileInfo_ioc(&pp->fm, pChannel, pa->pOutInfo);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

SYSCALL_2(truncate, const char* _Nonnull path, off_t length)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_TruncateFile(&pp->fm, pa->path, pa->length);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(ftruncate, int fd, off_t length)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pp->ioChannelTable, pa->fd, &pChannel)) == EOK) {
        err = FileManager_TruncateFile_ioc(&pp->fm, pChannel, pa->length);
        IOChannelTable_RelinquishChannel(&pp->ioChannelTable, pChannel);
    }
    return err;
}

SYSCALL_2(access, const char* _Nonnull path, int mode)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_CheckAccess(&pp->fm, pa->path, pa->mode);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(unlink, const char* _Nonnull path, int mode)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_Unlink(&pp->fm, pa->path, pa->mode);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(rename, const char* _Nonnull oldPath, const char* _Nonnull newPath)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_Rename(&pp->fm, pa->oldPath, pa->newPath);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_1(umask, mode_t mask)
{
    ProcessRef pp = vp->proc;
    mode_t omask;

    mtx_lock(&pp->mtx);
    if (pa->mask != SEO_UMASK_NO_CHANGE) {
        omask = FileManager_UMask(&pp->fm, pa->mask);
    }
    else {
        omask = FileManager_GetUMask(&pp->fm);
    }
    mtx_unlock(&pp->mtx);
    
    return (intptr_t)omask;
}

SYSCALL_3(chown, const char* _Nonnull path, uid_t uid, gid_t gid)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_SetFileOwner(&pp->fm, pa->path, pa->uid, pa->gid);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(chmod, const char* _Nonnull path, mode_t mode)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_SetFileMode(&pp->fm, pa->path, pa->mode);
    mtx_unlock(&pp->mtx);
    return err;
}

SYSCALL_2(utimens, const char* _Nonnull path, const struct timespec times[_Nullable 2])
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    mtx_lock(&pp->mtx);
    err = FileManager_SetFileTimestamps(&pp->fm, pa->path, pa->times);
    mtx_unlock(&pp->mtx);
    return err;
}
