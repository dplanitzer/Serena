//
//  Syscalls.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "Process.h"
#include "IOResource.h"
#include "VirtualProcessor.h"


struct SYS_mkfile_args {
    int                     scno;
    const char* _Nullable   path;
    unsigned int            options;
    uint32_t                permissions;
    int* _Nullable          outFd;
};

intptr_t _SYSCALL_mkfile(const struct SYS_mkfile_args* _Nonnull pArgs)
{
    decl_try_err();

    if (pArgs->path == NULL || pArgs->outFd == NULL) {
        return EINVAL;
    }

    return Process_CreateFile(Process_GetCurrent(), pArgs->path, pArgs->options, (FilePermissions)pArgs->permissions, pArgs->outFd);
}

struct SYS_open_args {
    int                     scno;
    const char* _Nullable   path;
    unsigned int            options;
    int* _Nullable          outFd;
};

intptr_t _SYSCALL_open(const struct SYS_open_args* _Nonnull pArgs)
{
    decl_try_err();
    IOResourceRef pConsole = NULL;
    IOChannelRef pChannel = NULL;

    if (pArgs->path == NULL || pArgs->outFd == NULL) {
        throw(EINVAL);
    }

    if (String_Equals(pArgs->path, "/dev/console")) {
        User user = {kRootUserId, kRootGroupId}; //XXX
        try_null(pConsole, (IOResourceRef) DriverManager_GetDriverForName(gDriverManager, kConsoleName), ENODEV);
        try(IOResource_Open(pConsole, NULL/*XXX*/, pArgs->options, user, &pChannel));
        try(Process_RegisterIOChannel(Process_GetCurrent(), pChannel, pArgs->outFd));
        Object_Release(pChannel);
    }
    else {
        try(Process_Open(Process_GetCurrent(), pArgs->path, pArgs->options, pArgs->outFd));
    }

    return EOK;

catch:
    Object_Release(pChannel);
    return err;
}


struct SYS_opendir_args {
    int                     scno;
    const char* _Nullable   path;
    int* _Nullable          outFd;
};

intptr_t _SYSCALL_opendir(const struct SYS_opendir_args* _Nonnull pArgs)
{
    if (pArgs->path == NULL || pArgs->outFd == NULL) {
        return EINVAL;
    }

    return Process_OpenDirectory(Process_GetCurrent(), pArgs->path, pArgs->outFd);
}


struct SYS_mkpipe_args {
    int             scno;
    int* _Nullable  pOutReadChannel;
    int* _Nullable  pOutWriteChannel;
};

intptr_t _SYSCALL_mkpipe(const struct SYS_mkpipe_args* _Nonnull pArgs)
{
    decl_try_err();

    if (pArgs->pOutReadChannel == NULL || pArgs->pOutWriteChannel == NULL) {
        return EINVAL;
    }

    return Process_CreatePipe(Process_GetCurrent(), pArgs->pOutReadChannel, pArgs->pOutWriteChannel);
}


struct SYS_close_args {
    int scno;
    int fd;
};

intptr_t _SYSCALL_close(const struct SYS_close_args* _Nonnull pArgs)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_UnregisterIOChannel(Process_GetCurrent(), pArgs->fd, &pChannel));
    // The error that close() returns is purely advisory and thus we'll proceed
    // with releasing the resource in any case.
    err = IOChannel_Close(pChannel);
    Object_Release(pChannel);

catch:
    return err;
}


struct SYS_read_args {
    int                 scno;
    int                 fd;
    void* _Nonnull      buffer;
    size_t              nBytesToRead;
    ssize_t* _Nonnull   nBytesRead;
};

intptr_t _SYSCALL_read(const struct SYS_read_args* _Nonnull pArgs)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_CopyIOChannelForDescriptor(Process_GetCurrent(), pArgs->fd, &pChannel));
    try(IOChannel_Read(pChannel, pArgs->buffer, __SSizeByClampingSize(pArgs->nBytesToRead), pArgs->nBytesRead));
    Object_Release(pChannel);
    return EOK;

catch:
    Object_Release(pChannel);
    return err;
}


struct SYS_write_args {
    int                     scno;
    int                     fd;
    const void* _Nonnull    buffer;
    size_t                  nBytesToWrite;
    ssize_t* _Nonnull       nBytesWritten;
};

intptr_t _SYSCALL_write(const struct SYS_write_args* _Nonnull pArgs)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_CopyIOChannelForDescriptor(Process_GetCurrent(), pArgs->fd, &pChannel));
    try(IOChannel_Write(pChannel, pArgs->buffer, __SSizeByClampingSize(pArgs->nBytesToWrite), pArgs->nBytesWritten));
    Object_Release(pChannel);
    return EOK;

catch:
    Object_Release(pChannel);
    return err;
}


struct SYS_seek_args {
    int                     scno;
    int                     fd;
    FileOffset              offset;
    FileOffset* _Nullable   outOldPosition;
    int                     whence;
};

intptr_t _SYSCALL_seek(const struct SYS_seek_args* _Nonnull pArgs)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_CopyIOChannelForDescriptor(Process_GetCurrent(), pArgs->fd, &pChannel));
    try(IOChannel_Seek(pChannel, pArgs->offset, pArgs->outOldPosition, pArgs->whence));
    Object_Release(pChannel);
    return EOK;

catch:
    Object_Release(pChannel);
    return err;
}


struct SYS_mkdir_args {
    int                     scno;
    const char* _Nullable   path;
    uint32_t                mode;   // XXX User space passes uint16_t, we need uint32_t because the C compiler for m68k does this kind of promotion
};

intptr_t _SYSCALL_mkdir(const struct SYS_mkdir_args* _Nonnull pArgs)
{
    decl_try_err();

    if (pArgs->path == NULL) {
        return ENOTDIR;
    }

    return Process_CreateDirectory(Process_GetCurrent(), pArgs->path, (FilePermissions) pArgs->mode);
}


struct SYS_getcwd_args {
    int             scno;
    char* _Nullable buffer;
    ssize_t         bufferSize;
};

intptr_t _SYSCALL_getcwd(const struct SYS_getcwd_args* _Nonnull pArgs)
{
    if (pArgs->buffer == NULL || pArgs->bufferSize < 0) {
        return EINVAL;
    }

    return Process_GetWorkingDirectory(Process_GetCurrent(), pArgs->buffer, pArgs->bufferSize);
}


struct SYS_setcwd_args {
    int                     scno;
    const char* _Nullable   path;
};

intptr_t _SYSCALL_setcwd(const struct SYS_setcwd_args* _Nonnull pArgs)
{
    if (pArgs->path == NULL) {
        return EINVAL;
    }

    return Process_SetWorkingDirectory(Process_GetCurrent(), pArgs->path);
}


struct SYS_getfileinfo_args {
    int                     scno;
    const char* _Nullable   path;
    FileInfo* _Nullable     outInfo;
};

intptr_t _SYSCALL_getfileinfo(const struct SYS_getfileinfo_args* _Nonnull pArgs)
{
    if (pArgs->path == NULL || pArgs->outInfo == NULL) {
        return EINVAL;
    }

    return Process_GetFileInfo(Process_GetCurrent(), pArgs->path, pArgs->outInfo);
}


struct SYS_setfileinfo_args {
    int                         scno;
    const char* _Nullable       path;
    MutableFileInfo* _Nullable  info;
};

intptr_t _SYSCALL_setfileinfo(const struct SYS_setfileinfo_args* _Nonnull pArgs)
{
    if (pArgs->path == NULL || pArgs->info == NULL) {
        return EINVAL;
    }

    return Process_SetFileInfo(Process_GetCurrent(), pArgs->path, pArgs->info);
}


struct SYS_fgetfileinfo_args {
    int                 scno;
    int                 fd;
    FileInfo* _Nullable outInfo;
};

intptr_t _SYSCALL_fgetfileinfo(const struct SYS_fgetfileinfo_args* _Nonnull pArgs)
{
    if (pArgs->outInfo == NULL) {
        return EINVAL;
    }

    return Process_GetFileInfoFromIOChannel(Process_GetCurrent(), pArgs->fd, pArgs->outInfo);
}


struct SYS_fsetfileinfo_args {
    int                         scno;
    int                         fd;
    MutableFileInfo* _Nullable  info;
};

intptr_t _SYSCALL_fsetfileinfo(const struct SYS_fsetfileinfo_args* _Nonnull pArgs)
{
    if (pArgs->info == NULL) {
        return EINVAL;
    }

    return Process_SetFileInfoFromIOChannel(Process_GetCurrent(), pArgs->fd, pArgs->info);
}


struct SYS_truncate_args {
    int                     scno;
    const char* _Nullable   path;
    FileOffset              length;
};

intptr_t _SYSCALL_truncate(const struct SYS_truncate_args* _Nonnull pArgs)
{
    if (pArgs->path == NULL) {
        return EINVAL;
    }

    return Process_TruncateFile(Process_GetCurrent(), pArgs->path, pArgs->length);
}


struct SYS_ftruncate_args {
    int         scno;
    int         fd;
    FileOffset  length;
};

intptr_t _SYSCALL_ftruncate(const struct SYS_ftruncate_args* _Nonnull pArgs)
{
    return Process_TruncateFileFromIOChannel(Process_GetCurrent(), pArgs->fd, pArgs->length);
}


struct SYS_ioctl_args {
    int                 scno;
    int                 fd;
    int                 cmd;
    va_list _Nullable   ap;
};

intptr_t _SYSCALL_ioctl(const struct SYS_ioctl_args* _Nonnull pArgs)
{
    return Process_vIOControl(Process_GetCurrent(), pArgs->fd, pArgs->cmd, pArgs->ap);
}


struct SYS_access_args {
    int                     scno;
    const char* _Nullable   path;
    uint32_t                mode;
};

intptr_t _SYSCALL_access(const struct SYS_access_args* _Nonnull pArgs)
{
    if (pArgs->path == NULL) {
        return EINVAL;
    }

    return Process_CheckFileAccess(Process_GetCurrent(), pArgs->path, pArgs->mode);
}


struct SYS_unlink_args {
    int                     scno;
    const char* _Nullable   path;
};

intptr_t _SYSCALL_unlink(const struct SYS_unlink_args* _Nonnull pArgs)
{
    if (pArgs->path == NULL) {
        return EINVAL;
    }

    return Process_Unlink(Process_GetCurrent(), pArgs->path);
}


struct SYS_rename_args {
    int                     scno;
    const char* _Nullable   oldPath;
    const char* _Nullable   newPath;
};

intptr_t _SYSCALL_rename(const struct SYS_rename_args* _Nonnull pArgs)
{
    if (pArgs->oldPath == NULL || pArgs->newPath == NULL) {
        return EINVAL;
    }

    return Process_Rename(Process_GetCurrent(), pArgs->oldPath, pArgs->newPath);
}


intptr_t _SYSCALL_getumask(void)
{
    return (int) Process_GetFileCreationMask(Process_GetCurrent());
}


struct SYS_setumask_args {
    int         scno;
    uint16_t    mask;
};

intptr_t _SYSCALL_setumask(const struct SYS_setumask_args* _Nonnull pArgs)
{
    Process_SetFileCreationMask(Process_GetCurrent(), pArgs->mask);
    return EOK;
}


struct SYS_delay_args {
    int             scno;
    TimeInterval    delay;
};

intptr_t _SYSCALL_delay(const struct SYS_delay_args* _Nonnull pArgs)
{
    if (pArgs->delay.tv_nsec < 0 || pArgs->delay.tv_nsec >= ONE_SECOND_IN_NANOS) {
        return EINVAL;
    }

    return VirtualProcessor_Sleep(pArgs->delay);
}


struct SYS_dispatch_async_args {
    int                             scno;
    const Closure1Arg_Func _Nonnull userClosure;
};

intptr_t _SYSCALL_dispatch_async(const struct SYS_dispatch_async_args* pArgs)
{
    decl_try_err();

    throw_ifnull(pArgs->userClosure, EINVAL);
    try(Process_DispatchAsyncUser(Process_GetCurrent(), pArgs->userClosure));
    return EOK;

catch:
    return err;
}


// Allocates more address space to the calling process. The address space is
// expanded by 'count' bytes. A pointer to the first byte in the newly allocated
// address space portion is return in 'pOutMem'. 'pOutMem' is set to NULL and a
// suitable error is returned if the allocation failed. 'count' must be greater
// than 0 and a multiple of the CPU page size.
struct SYS_alloc_address_space_args {
    int                         scno;
    size_t                      nbytes;
    void * _Nullable * _Nonnull pOutMem;
};

intptr_t _SYSCALL_alloc_address_space(struct SYS_alloc_address_space_args* _Nonnull pArgs)
{
    decl_try_err();

    if (pArgs->nbytes > SSIZE_MAX) {
        throw(E2BIG);
    }
    throw_ifnull(pArgs->pOutMem, EINVAL);

    try(Process_AllocateAddressSpace(Process_GetCurrent(),
        __SSizeByClampingSize(pArgs->nbytes),
        pArgs->pOutMem));
    return EOK;

catch:
    return err;
}


struct SYS_exit_args {
    int scno;
    int status;
};

intptr_t _SYSCALL_exit(const struct SYS_exit_args* _Nonnull pArgs)
{
    // Trigger the termination of the process. Note that the actual termination
    // is done asynchronously. That's why we sleep below since we don't want to
    // return to user space anymore.
    Process_Terminate(Process_GetCurrent(), pArgs->status);


    // This wait here will eventually be aborted when the dispatch queue that
    // owns this VP is terminated. This interrupt will be caused by the abort
    // of the call-as-user and thus this system call will not return to user
    // space anymore. Instead it will return to the dispatch queue main loop.
    VirtualProcessor_Sleep(kTimeInterval_Infinity);
    return EOK;
}


// Spawns a new process which is made the child of the process that is invoking
// this system call. The 'argv' pointer points to a table that holds pointers to
// nul-terminated strings. The last entry in the table has to be NULL. All these
// strings are the command line arguments that should be passed to the new
// process.
struct SYS_spawn_process_args {
    int                             scno;
    const SpawnArguments* _Nullable spawnArgs;
    ProcessId* _Nullable            pOutPid;
};

intptr_t _SYSCALL_spawn_process(const struct SYS_spawn_process_args* pArgs)
{
    decl_try_err();

    throw_ifnull(pArgs->spawnArgs, EINVAL);
    try(Process_SpawnChildProcess(Process_GetCurrent(), pArgs->spawnArgs, pArgs->pOutPid));
    return EOK;

catch:
    return err;
}


intptr_t _SYSCALL_getpid(void)
{
    return Process_GetId(Process_GetCurrent());
}


intptr_t _SYSCALL_getppid(void)
{
    return Process_GetParentId(Process_GetCurrent());
}


intptr_t _SYSCALL_getuid(void)
{
    return Process_GetRealUserId(Process_GetCurrent());
}


intptr_t _SYSCALL_getpargs(void)
{
    return (int) Process_GetArgumentsBaseAddress(Process_GetCurrent());
}


struct SYS_waitpid_args {
    int                                 scno;
    ProcessId                           pid;
    ProcessTerminationStatus* _Nullable status;
};

intptr_t _SYSCALL_waitpid(struct SYS_waitpid_args* pArgs)
{
    return Process_WaitForTerminationOfChild(Process_GetCurrent(), pArgs->pid, pArgs->status);
}