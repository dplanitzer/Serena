//
//  file.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <filemanager/FilesystemManager.h>
#include <filesystem/IOChannel.h>
#include <time.h>
#include <kern/limits.h>
#include <kern/timespec.h>
#include <kpi/disk.h>
#include <kpi/fs.h>
#include <kpi/uid.h>


SYSCALL_4(mkfile, const char* _Nonnull path, unsigned int mode, uint32_t permissions, int* _Nonnull pOutIoc)
{
    return Process_CreateFile((ProcessRef)p, pa->path, pa->mode, (mode_t)pa->permissions, pa->pOutIoc);
}

SYSCALL_3(open, const char* _Nonnull path, unsigned int mode, int* _Nonnull pOutIoc)
{
    return Process_OpenFile((ProcessRef)p, pa->path, pa->mode, pa->pOutIoc);
}

SYSCALL_2(opendir, const char* _Nonnull path, int* _Nonnull pOutIoc)
{
    return Process_OpenDirectory((ProcessRef)p, pa->path, pa->pOutIoc);
}

SYSCALL_2(mkpipe, int* _Nonnull  pOutReadChannel, int* _Nonnull  pOutWriteChannel)
{
    return Process_CreatePipe((ProcessRef)p, pa->pOutReadChannel, pa->pOutWriteChannel);
}

SYSCALL_2(mkdir, const char* _Nonnull path, uint32_t mode)
{
    return Process_CreateDirectory((ProcessRef)p, pa->path, (mode_t) pa->mode);
}

SYSCALL_2(getcwd, char* _Nonnull buffer, size_t bufferSize)
{
    return Process_GetWorkingDirectoryPath((ProcessRef)p, pa->buffer, pa->bufferSize);
}

SYSCALL_1(chdir, const char* _Nonnull path)
{
    return Process_SetWorkingDirectoryPath((ProcessRef)p, pa->path);
}

SYSCALL_2(stat, const char* _Nonnull path, struct stat* _Nonnull pOutInfo)
{
    return Process_GetFileInfo((ProcessRef)p, pa->path, pa->pOutInfo);
}

SYSCALL_2(fstat, int ioc, struct stat* _Nonnull pOutInfo)
{
    return Process_GetFileInfo_ioc((ProcessRef)p, pa->ioc, pa->pOutInfo);
}

SYSCALL_2(truncate, const char* _Nonnull path, off_t length)
{
    return Process_TruncateFile((ProcessRef)p, pa->path, pa->length);
}

SYSCALL_2(ftruncate, int ioc, off_t length)
{
    return Process_TruncateFile_ioc((ProcessRef)p, pa->ioc, pa->length);
}

SYSCALL_2(access, const char* _Nonnull path, uint32_t mode)
{
    return Process_CheckAccess((ProcessRef)p, pa->path, pa->mode);
}

SYSCALL_2(unlink, const char* _Nonnull path, int mode)
{
    return Process_Unlink((ProcessRef)p, pa->path, pa->mode);
}

SYSCALL_2(rename, const char* _Nonnull oldPath, const char* _Nonnull newPath)
{
    return Process_Rename((ProcessRef)p, pa->oldPath, pa->newPath);
}

SYSCALL_1(umask, mode_t mask)
{
    return (intptr_t)Process_UMask((ProcessRef)p, pa->mask);
}

SYSCALL_4(mount, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    return Process_Mount((ProcessRef)p, pa->objectType, pa->objectName, pa->atDirPath, pa->params);
}

SYSCALL_2(unmount, const char* _Nonnull atDirPath, UnmountOptions options)
{
    return Process_Unmount((ProcessRef)p, pa->atDirPath, pa->options);
}

SYSCALL_0(sync)
{
    FilesystemManager_Sync(gFilesystemManager);
    return EOK;
}

SYSCALL_3(fsgetdisk, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    return Process_GetFilesystemDiskPath((ProcessRef)p, pa->fsid, pa->buf, pa->bufSize);
}

SYSCALL_3(chown, const char* _Nonnull path, uid_t uid, gid_t gid)
{
    return Process_SetFileOwner((ProcessRef)p, pa->path, pa->uid, pa->gid);
}

SYSCALL_2(chmod, const char* _Nonnull path, mode_t mode)
{
    return Process_SetFileMode((ProcessRef)p, pa->path, pa->mode);
}

SYSCALL_2(utimens, const char* _Nonnull path, const struct timespec times[_Nullable 2])
{
    return Process_SetFileTimestamps((ProcessRef)p, pa->path, pa->times);
}
