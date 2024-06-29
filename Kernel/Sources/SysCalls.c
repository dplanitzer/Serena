//
//  SysCalls.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <dispatcher/VirtualProcessor.h>
#include <dispatchqueue/DispatchQueue.h>
#include <process/Process.h>
#include "IOChannel.h"
#include "User.h"

typedef intptr_t (*SystemCall)(void* _Nonnull);

#define REF_SYSCALL(__name) \
    (SystemCall)_SYSCALL_##__name

#define SYSCALL_0(__name) \
    struct args##__name { \
        int scno; \
    }; \
    intptr_t _SYSCALL_##__name(const struct args##__name* _Nonnull pArgs)

#define SYSCALL_1(__name, __p1) \
    struct args##__name { \
        int scno; \
        __p1; \
    }; \
    intptr_t _SYSCALL_##__name(const struct args##__name* _Nonnull pArgs)

#define SYSCALL_2(__name, __p1, __p2) \
    struct args##__name { \
        int scno; \
        __p1; \
        __p2; \
    }; \
    intptr_t _SYSCALL_##__name(const struct args##__name* _Nonnull pArgs)

#define SYSCALL_3(__name, __p1, __p2, __p3) \
    struct args##__name { \
        int scno; \
        __p1; \
        __p2; \
        __p3; \
    }; \
    intptr_t _SYSCALL_##__name(const struct args##__name* _Nonnull pArgs)

#define SYSCALL_4(__name, __p1, __p2, __p3, __p4) \
    struct args_##__name { \
        int scno; \
        __p1; \
        __p2; \
        __p3; \
        __p4; \
    }; \
    intptr_t _SYSCALL_##__name(const struct args_##__name* _Nonnull pArgs)

#define SYSCALL_5(__name, __p1, __p2, __p3, __p4, __p5) \
    struct args_##__name { \
        int scno; \
        __p1; \
        __p2; \
        __p3; \
        __p4; \
        __p5; \
    }; \
    intptr_t _SYSCALL_##__name(const struct args_##__name* _Nonnull pArgs)


////////////////////////////////////////////////////////////////////////////////

SYSCALL_4(mkfile, const char* _Nullable path, unsigned options, uint32_t permissions, int* _Nullable pOutIoc)
{
    if (pArgs->path == NULL || pArgs->pOutIoc == NULL) {
        return EINVAL;
    }

    return Process_CreateFile(Process_GetCurrent(), pArgs->path, pArgs->options, (FilePermissions)pArgs->permissions, pArgs->pOutIoc);
}

SYSCALL_3(open, const char* _Nullable path, unsigned int options, int* _Nullable pOutIoc)
{
    if (pArgs->path == NULL || pArgs->pOutIoc == NULL) {
        return EINVAL;
    }

    return Process_OpenFile(Process_GetCurrent(), pArgs->path, pArgs->options, pArgs->pOutIoc);
}

SYSCALL_2(opendir, const char* _Nullable path, int* _Nullable pOutIoc)
{
    if (pArgs->path == NULL || pArgs->pOutIoc == NULL) {
        return EINVAL;
    }

    return Process_OpenDirectory(Process_GetCurrent(), pArgs->path, pArgs->pOutIoc);
}

SYSCALL_2(mkpipe, int* _Nullable  pOutReadChannel, int* _Nullable  pOutWriteChannel)
{
    if (pArgs->pOutReadChannel == NULL || pArgs->pOutWriteChannel == NULL) {
        return EINVAL;
    }

    return Process_CreatePipe(Process_GetCurrent(), pArgs->pOutReadChannel, pArgs->pOutWriteChannel);
}

SYSCALL_1(close, int ioc)
{
    return Process_CloseChannel(Process_GetCurrent(), pArgs->ioc);
}

SYSCALL_4(read, int ioc, void* _Nullable buffer, size_t nBytesToRead, ssize_t* _Nullable nBytesRead)
{
    if (pArgs->buffer == NULL || pArgs->nBytesRead == NULL) {
        return EINVAL;
    }

    return Process_ReadChannel(Process_GetCurrent(), pArgs->ioc, pArgs->buffer, pArgs->nBytesToRead, pArgs->nBytesRead);
}

SYSCALL_4(write, int ioc, const void* _Nullable buffer, size_t nBytesToWrite, ssize_t* _Nullable nBytesWritten)
{
    if (pArgs->buffer == NULL || pArgs->nBytesWritten == NULL) {
        return EINVAL;
    }

    return Process_WriteChannel(Process_GetCurrent(), pArgs->ioc, pArgs->buffer, pArgs->nBytesToWrite, pArgs->nBytesWritten);
}

SYSCALL_4(seek, int ioc, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
{
    return Process_SeekChannel(Process_GetCurrent(), pArgs->ioc, pArgs->offset, pArgs->pOutOldPosition, pArgs->whence);
}

SYSCALL_2(mkdir, const char* _Nullable path, uint32_t mode)
{
    if (pArgs->path == NULL) {
        return ENOTDIR;
    }

    return Process_CreateDirectory(Process_GetCurrent(), pArgs->path, (FilePermissions) pArgs->mode);
}

SYSCALL_2(getcwd, char* _Nullable buffer, size_t bufferSize)
{
    if (pArgs->buffer == NULL) {
        return EINVAL;
    }

    return Process_GetWorkingDirectoryPath(Process_GetCurrent(), pArgs->buffer, pArgs->bufferSize);
}

SYSCALL_1(setcwd, const char* _Nullable path)
{
    if (pArgs->path == NULL) {
        return EINVAL;
    }

    return Process_SetWorkingDirectoryPath(Process_GetCurrent(), pArgs->path);
}

SYSCALL_2(getfileinfo, const char* _Nullable path, FileInfo* _Nullable pOutInfo)
{
    if (pArgs->path == NULL || pArgs->pOutInfo == NULL) {
        return EINVAL;
    }

    return Process_GetFileInfo(Process_GetCurrent(), pArgs->path, pArgs->pOutInfo);
}

SYSCALL_2(setfileinfo, const char* _Nullable path, MutableFileInfo* _Nullable pInfo)
{
    if (pArgs->path == NULL || pArgs->pInfo == NULL) {
        return EINVAL;
    }

    return Process_SetFileInfo(Process_GetCurrent(), pArgs->path, pArgs->pInfo);
}

SYSCALL_2(fgetfileinfo, int ioc, FileInfo* _Nullable pOutInfo)
{
    if (pArgs->pOutInfo == NULL) {
        return EINVAL;
    }

    return Process_GetFileInfoFromIOChannel(Process_GetCurrent(), pArgs->ioc, pArgs->pOutInfo);
}

SYSCALL_2(fsetfileinfo, int ioc, MutableFileInfo* _Nullable pInfo)
{
    if (pArgs->pInfo == NULL) {
        return EINVAL;
    }

    return Process_SetFileInfoFromIOChannel(Process_GetCurrent(), pArgs->ioc, pArgs->pInfo);
}

SYSCALL_2(truncate, const char* _Nullable path, FileOffset length)
{
    if (pArgs->path == NULL) {
        return EINVAL;
    }

    return Process_TruncateFile(Process_GetCurrent(), pArgs->path, pArgs->length);
}

SYSCALL_2(ftruncate, int ioc, FileOffset length)
{
    return Process_TruncateFileFromIOChannel(Process_GetCurrent(), pArgs->ioc, pArgs->length);
}

SYSCALL_3(ioctl, int ioc, int cmd, va_list _Nullable ap)
{
    return Process_vIOControl(Process_GetCurrent(), pArgs->ioc, pArgs->cmd, pArgs->ap);
}

SYSCALL_2(access, const char* _Nullable path, uint32_t mode)
{
    if (pArgs->path == NULL) {
        return EINVAL;
    }

    return Process_CheckFileAccess(Process_GetCurrent(), pArgs->path, pArgs->mode);
}

SYSCALL_1(unlink, const char* _Nullable path)
{
    return (pArgs->path) ? Process_Unlink(Process_GetCurrent(), pArgs->path) : EINVAL;
}

SYSCALL_2(rename, const char* _Nullable oldPath, const char* _Nullable newPath)
{
    if (pArgs->oldPath == NULL || pArgs->newPath == NULL) {
        return EINVAL;
    }

    return Process_Rename(Process_GetCurrent(), pArgs->oldPath, pArgs->newPath);
}


SYSCALL_0(getumask)
{
    return Process_GetFileCreationMask(Process_GetCurrent());
}

SYSCALL_1(setumask, uint32_t mask)
{
    Process_SetFileCreationMask(Process_GetCurrent(), pArgs->mask);
    return EOK;
}

SYSCALL_1(delay, TimeInterval delay)
{
    if (pArgs->delay.tv_nsec < 0 || pArgs->delay.tv_nsec >= ONE_SECOND_IN_NANOS) {
        return EINVAL;
    }

    return VirtualProcessor_Sleep(pArgs->delay);
}

SYSCALL_1(get_monotonic_time, TimeInterval* _Nullable time)
{
    if (pArgs->time == NULL) {
        return EINVAL;
    }

    *(pArgs->time) = MonotonicClock_GetCurrentTime();
    return EOK;
}

SYSCALL_4(dispatch, int od, unsigned long options, const Closure1Arg_Func _Nullable pUserClosure, void* _Nullable pContext)
{
    if (pArgs->pUserClosure == NULL) {
        return EINVAL;
    }

    return Process_DispatchUserClosure(Process_GetCurrent(), pArgs->od, pArgs->options, pArgs->pUserClosure, pArgs->pContext);
}

SYSCALL_4(dispatch_after, int od, TimeInterval deadline, const Closure1Arg_Func _Nullable pUserClosure, void* _Nullable pContext)
{
    if (pArgs->pUserClosure == NULL) {
        return EINVAL;
    }

    return Process_DispatchUserClosureAsyncAfter(Process_GetCurrent(), pArgs->od, pArgs->deadline, pArgs->pUserClosure, pArgs->pContext);
}

SYSCALL_5(dispatch_queue_create, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nullable pOutQueue)
{
    if (pArgs->pOutQueue == NULL) {
        return EINVAL;
    }

    return Process_CreateDispatchQueue(Process_GetCurrent(), pArgs->minConcurrency, pArgs->maxConcurrency, pArgs->qos, pArgs->priority, pArgs->pOutQueue);
}

SYSCALL_0(dispatch_queue_current)
{
    return Process_GetCurrentDispatchQueue(Process_GetCurrent());
}


SYSCALL_1(cv_create, int* _Nullable pOutOd)
{
    if (pArgs->pOutOd == NULL) {
        return EINVAL;
    }

    return Process_CreateUConditionVariable(Process_GetCurrent(), pArgs->pOutOd);
}

SYSCALL_3(cv_wake, int od, int dlock, unsigned int options)
{
    return Process_WakeUConditionVariable(Process_GetCurrent(), pArgs->od, pArgs->dlock, pArgs->options);
}

SYSCALL_3(cv_wait, int od, int dlock, TimeInterval deadline)
{
    return Process_WaitUConditionVariable(Process_GetCurrent(), pArgs->od, pArgs->dlock, pArgs->deadline);
}


SYSCALL_1(lock_create, int* _Nullable pOutOd)
{
    if (pArgs->pOutOd == NULL) {
        return EINVAL;
    }

    return Process_CreateULock(Process_GetCurrent(), pArgs->pOutOd);
}

SYSCALL_1(lock_trylock, int od)
{
    return Process_TryULock(Process_GetCurrent(), pArgs->od);
}

SYSCALL_1(lock_lock, int od)
{
    return Process_LockULock(Process_GetCurrent(), pArgs->od);
}

SYSCALL_1(lock_unlock, int od)
{
    return Process_UnlockULock(Process_GetCurrent(), pArgs->od);
}


SYSCALL_2(sema_create, int npermits, int* _Nullable pOutOd)
{
    if (pArgs->pOutOd == NULL) {
        return EINVAL;
    }

    return Process_CreateUSemaphore(Process_GetCurrent(), pArgs->npermits, pArgs->pOutOd);
}

SYSCALL_2(sema_relinquish, int od, int npermits)
{
    return Process_RelinquishUSemaphore(Process_GetCurrent(), pArgs->od, pArgs->npermits);
}

SYSCALL_3(sema_acquire, int od, int npermits, TimeInterval deadline)
{
    return Process_AcquireUSemaphore(Process_GetCurrent(), pArgs->od, pArgs->npermits, pArgs->deadline);
}

SYSCALL_2(sema_tryacquire, int od, int npermits)
{
    return Process_TryAcquireUSemaphore(Process_GetCurrent(), pArgs->od, pArgs->npermits);
}


SYSCALL_1(dispose, int od)
{
    return Process_DisposeUResource(Process_GetCurrent(), pArgs->od);
}

// Allocates more address space to the calling process. The address space is
// expanded by 'count' bytes. A pointer to the first byte in the newly allocated
// address space portion is return in 'pOutMem'. 'pOutMem' is set to NULL and a
// suitable error is returned if the allocation failed. 'count' must be greater
// than 0 and a multiple of the CPU page size.
SYSCALL_2(alloc_address_space, size_t nbytes, void * _Nullable * _Nonnull pOutMem)
{
    if (pArgs->nbytes > SSIZE_MAX) {
        return E2BIG;
    }
    if (pArgs->pOutMem == NULL) {
        return EINVAL;
    }

    return Process_AllocateAddressSpace(Process_GetCurrent(),
        __SSizeByClampingSize(pArgs->nbytes),
        pArgs->pOutMem);
}

SYSCALL_1(exit, int status)
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
SYSCALL_4(spawn_process, const char* _Nullable path, const char* _Nullable * _Nullable argv, const SpawnOptions* _Nullable options, ProcessId* _Nullable pOutPid)
{
    if (pArgs->path == NULL || pArgs->path[0] == '\0') {
        return EINVAL;
    }

    return Process_SpawnChildProcess(Process_GetCurrent(), pArgs->path, pArgs->argv, pArgs->options, pArgs->pOutPid);
}

SYSCALL_0(getpid)
{
    return Process_GetId(Process_GetCurrent());
}

SYSCALL_0(getppid)
{
    return Process_GetParentId(Process_GetCurrent());
}

SYSCALL_0(getuid)
{
    return Process_GetRealUserId(Process_GetCurrent());
}

SYSCALL_0(getpargs)
{
    return (intptr_t) Process_GetArgumentsBaseAddress(Process_GetCurrent());
}

SYSCALL_2(waitpid, ProcessId pid, ProcessTerminationStatus* _Nullable pOutStatus)
{
    return Process_WaitForTerminationOfChild(Process_GetCurrent(), pArgs->pid, pArgs->pOutStatus);
}


SystemCall gSystemCallTable[] = {
    REF_SYSCALL(read),
    REF_SYSCALL(write),
    REF_SYSCALL(delay),
    REF_SYSCALL(dispatch),
    REF_SYSCALL(alloc_address_space),
    REF_SYSCALL(exit),
    REF_SYSCALL(spawn_process),
    REF_SYSCALL(getpid),
    REF_SYSCALL(getppid),
    REF_SYSCALL(getpargs),
    REF_SYSCALL(open),
    REF_SYSCALL(close),
    REF_SYSCALL(waitpid),
    REF_SYSCALL(seek),
    REF_SYSCALL(getcwd),
    REF_SYSCALL(setcwd),
    REF_SYSCALL(getuid),
    REF_SYSCALL(getumask),
    REF_SYSCALL(setumask),
    REF_SYSCALL(mkdir),
    REF_SYSCALL(getfileinfo),
    REF_SYSCALL(opendir),
    REF_SYSCALL(setfileinfo),
    REF_SYSCALL(access),
    REF_SYSCALL(fgetfileinfo),
    REF_SYSCALL(fsetfileinfo),
    REF_SYSCALL(unlink),
    REF_SYSCALL(rename),
    REF_SYSCALL(ioctl),
    REF_SYSCALL(truncate),
    REF_SYSCALL(ftruncate),
    REF_SYSCALL(mkfile),
    REF_SYSCALL(mkpipe),
    REF_SYSCALL(dispatch_after),
    REF_SYSCALL(dispatch_queue_create),
    REF_SYSCALL(dispatch_queue_current),
    REF_SYSCALL(dispose),
    REF_SYSCALL(get_monotonic_time),
    REF_SYSCALL(lock_create),
    REF_SYSCALL(lock_trylock),
    REF_SYSCALL(lock_lock),
    REF_SYSCALL(lock_unlock),
    REF_SYSCALL(sema_create),
    REF_SYSCALL(sema_relinquish),
    REF_SYSCALL(sema_acquire),
    REF_SYSCALL(sema_tryacquire),
    REF_SYSCALL(cv_create),
    REF_SYSCALL(cv_wake),
    REF_SYSCALL(cv_wait),
};
