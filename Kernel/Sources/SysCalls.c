//
//  SysCalls.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <dispatcher/VirtualProcessor.h>
#include <dispatchqueue/DispatchQueue.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/IOChannel.h>
#include <hal/MonotonicClock.h>
#include <process/Process.h>
#include <System/Clock.h>
#include <System/Disk.h>
#include <System/Filesystem.h>
#include <System/User.h>


typedef intptr_t (*syscall_t)(ProcessRef _Nonnull proc, void* _Nonnull);

#define SYSCALL_COUNT   56
static syscall_t gSystemCallTable[SYSCALL_COUNT];


#define REF_SYSCALL(__name) \
(syscall_t)_SYSCALL_##__name

#define SYSCALL_0(__name) \
struct args##__name { \
    unsigned int scno; \
}; \
intptr_t _SYSCALL_##__name(ProcessRef _Nonnull proc, const struct args##__name* _Nonnull pa)

#define SYSCALL_1(__name, __p1) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
}; \
intptr_t _SYSCALL_##__name(ProcessRef _Nonnull proc, const struct args##__name* _Nonnull pa)

#define SYSCALL_2(__name, __p1, __p2) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
}; \
intptr_t _SYSCALL_##__name(ProcessRef _Nonnull proc, const struct args##__name* _Nonnull pa)

#define SYSCALL_3(__name, __p1, __p2, __p3) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
}; \
intptr_t _SYSCALL_##__name(ProcessRef _Nonnull proc, const struct args##__name* _Nonnull pa)

#define SYSCALL_4(__name, __p1, __p2, __p3, __p4) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
}; \
intptr_t _SYSCALL_##__name(ProcessRef _Nonnull proc, const struct args_##__name* _Nonnull pa)

#define SYSCALL_5(__name, __p1, __p2, __p3, __p4, __p5) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
    __p5; \
}; \
intptr_t _SYSCALL_##__name(ProcessRef _Nonnull proc, const struct args_##__name* _Nonnull pa)

#define SYSCALL_6(__name, __p1, __p2, __p3, __p4, __p5, __p6) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
    __p5; \
    __p6; \
}; \
intptr_t _SYSCALL_##__name(ProcessRef _Nonnull proc, const struct args_##__name* _Nonnull pa)


////////////////////////////////////////////////////////////////////////////////

intptr_t _syscall_handler(unsigned int* _Nonnull args)
{
    VirtualProcessor* vcpu = VirtualProcessor_GetCurrent();
    ProcessRef curProc = DispatchQueue_GetOwningProcess(vcpu->dispatchQueue);
    const unsigned int nscno = sizeof(gSystemCallTable) / sizeof(syscall_t);
    const unsigned int scno = *args;

    if (scno < nscno) {
        syscall_t sc = gSystemCallTable[scno];
        intptr_t r = sc(curProc, args);

        return r;
    }
    else {
        return ENOSYS;
    }
}


SYSCALL_4(mkfile, const char* _Nonnull path, unsigned int mode, uint32_t permissions, int* _Nonnull pOutIoc)
{
    return Process_CreateFile(proc, pa->path, pa->mode, (FilePermissions)pa->permissions, pa->pOutIoc);
}

SYSCALL_3(open, const char* _Nonnull path, unsigned int mode, int* _Nonnull pOutIoc)
{
    return Process_OpenFile(proc, pa->path, pa->mode, pa->pOutIoc);
}

SYSCALL_2(opendir, const char* _Nonnull path, int* _Nonnull pOutIoc)
{
    return Process_OpenDirectory(proc, pa->path, pa->pOutIoc);
}

SYSCALL_2(mkpipe, int* _Nonnull  pOutReadChannel, int* _Nonnull  pOutWriteChannel)
{
    return Process_CreatePipe(proc, pa->pOutReadChannel, pa->pOutWriteChannel);
}

SYSCALL_1(close, int ioc)
{
    return Process_CloseChannel(proc, pa->ioc);
}

SYSCALL_4(read, int ioc, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    return Process_ReadChannel(proc, pa->ioc, pa->buffer, pa->nBytesToRead, pa->nBytesRead);
}

SYSCALL_4(write, int ioc, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    return Process_WriteChannel(proc, pa->ioc, pa->buffer, pa->nBytesToWrite, pa->nBytesWritten);
}

SYSCALL_4(seek, int ioc, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    return Process_SeekChannel(proc, pa->ioc, pa->offset, pa->pOutOldPosition, pa->whence);
}

SYSCALL_2(mkdir, const char* _Nonnull path, uint32_t mode)
{
    return Process_CreateDirectory(proc, pa->path, (FilePermissions) pa->mode);
}

SYSCALL_2(getcwd, char* _Nonnull buffer, size_t bufferSize)
{
    return Process_GetWorkingDirectoryPath(proc, pa->buffer, pa->bufferSize);
}

SYSCALL_1(setcwd, const char* _Nonnull path)
{
    return Process_SetWorkingDirectoryPath(proc, pa->path);
}

SYSCALL_2(getfinfo, const char* _Nonnull path, finfo_t* _Nonnull pOutInfo)
{
    return Process_GetFileInfo(proc, pa->path, pa->pOutInfo);
}

SYSCALL_2(setfinfo, const char* _Nonnull path, fmutinfo_t* _Nonnull pInfo)
{
    return Process_SetFileInfo(proc, pa->path, pa->pInfo);
}

SYSCALL_2(fgetfinfo, int ioc, finfo_t* _Nonnull pOutInfo)
{
    return Process_GetFileInfo_ioc(proc, pa->ioc, pa->pOutInfo);
}

SYSCALL_2(fsetfinfo, int ioc, fmutinfo_t* _Nonnull pInfo)
{
    return Process_SetFileInfo_ioc(proc, pa->ioc, pa->pInfo);
}

SYSCALL_2(truncate, const char* _Nonnull path, off_t length)
{
    return Process_TruncateFile(proc, pa->path, pa->length);
}

SYSCALL_2(ftruncate, int ioc, off_t length)
{
    return Process_TruncateFile_ioc(proc, pa->ioc, pa->length);
}

SYSCALL_3(ioctl, int ioc, int cmd, va_list _Nullable ap)
{
    return Process_Iocall(proc, pa->ioc, pa->cmd, pa->ap);
}

SYSCALL_2(access, const char* _Nonnull path, uint32_t mode)
{
    return Process_CheckAccess(proc, pa->path, pa->mode);
}

SYSCALL_1(unlink, const char* _Nonnull path)
{
    return Process_Unlink(proc, pa->path);
}

SYSCALL_2(rename, const char* _Nonnull oldPath, const char* _Nonnull newPath)
{
    return Process_Rename(proc, pa->oldPath, pa->newPath);
}


SYSCALL_0(getumask)
{
    return Process_GetFileCreationMask(proc);
}

SYSCALL_1(setumask, uint32_t mask)
{
    Process_SetFileCreationMask(proc, pa->mask);
    return EOK;
}

SYSCALL_2(clock_wait, int clock, const TimeInterval* _Nonnull delay)
{
    if (pa->delay->tv_nsec < 0 || pa->delay->tv_nsec >= ONE_SECOND_IN_NANOS) {
        return EINVAL;
    }
    if (pa->clock != CLOCK_UPTIME) {
        return ENODEV;
    }

    return VirtualProcessor_Sleep(*(pa->delay));
}

SYSCALL_2(clock_gettime, int clock, TimeInterval* _Nonnull time)
{
    if (pa->clock != CLOCK_UPTIME) {
        return ENODEV;
    }

    *(pa->time) = MonotonicClock_GetCurrentTime();
    return EOK;
}

SYSCALL_5(dispatch, int od, const VoidFunc_2 _Nonnull func, void* _Nullable ctx, uint32_t options, uintptr_t tag)
{
    return Process_DispatchUserClosure(proc, pa->od, pa->func, pa->ctx, pa->options, pa->tag);
}

SYSCALL_6(dispatch_timer, int od, TimeInterval deadline, TimeInterval interval, const VoidFunc_1 _Nonnull func, void* _Nullable ctx, uintptr_t tag)
{
    return Process_DispatchUserTimer(proc, pa->od, pa->deadline, pa->interval, pa->func, pa->ctx, pa->tag);
}

SYSCALL_5(dispatch_queue_create, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
{
    return Process_CreateDispatchQueue(proc, pa->minConcurrency, pa->maxConcurrency, pa->qos, pa->priority, pa->pOutQueue);
}

SYSCALL_2(dispatch_remove_by_tag, int od, uintptr_t tag)
{
    return Process_DispatchRemoveByTag(proc, pa->od, pa->tag);
}

SYSCALL_0(dispatch_queue_current)
{
    return Process_GetCurrentDispatchQueue(proc);
}


SYSCALL_1(cond_create, int* _Nonnull pOutOd)
{
    return Process_CreateUConditionVariable(proc, pa->pOutOd);
}

SYSCALL_3(cond_wake, int od, int dlock, unsigned int options)
{
    return Process_WakeUConditionVariable(proc, pa->od, pa->dlock, pa->options);
}

SYSCALL_3(cond_timedwait, int od, int dlock, const TimeInterval* _Nullable deadline)
{
    const TimeInterval ti = (pa->deadline) ? *(pa->deadline) : kTimeInterval_Infinity;

    return Process_WaitUConditionVariable(proc, pa->od, pa->dlock, ti);
}


SYSCALL_1(lock_create, int* _Nonnull pOutOd)
{
    return Process_CreateULock(proc, pa->pOutOd);
}

SYSCALL_1(lock_trylock, int od)
{
    return Process_TryULock(proc, pa->od);
}

SYSCALL_1(lock_lock, int od)
{
    return Process_LockULock(proc, pa->od);
}

SYSCALL_1(lock_unlock, int od)
{
    return Process_UnlockULock(proc, pa->od);
}


SYSCALL_2(sem_create, int npermits, int* _Nonnull pOutOd)
{
    return Process_CreateUSemaphore(proc, pa->npermits, pa->pOutOd);
}

SYSCALL_2(sem_post, int od, int npermits)
{
    return Process_RelinquishUSemaphore(proc, pa->od, pa->npermits);
}

SYSCALL_3(sem_wait, int od, int npermits, TimeInterval deadline)
{
    return Process_AcquireUSemaphore(proc, pa->od, pa->npermits, pa->deadline);
}

SYSCALL_2(sem_trywait, int od, int npermits)
{
    return Process_TryAcquireUSemaphore(proc, pa->od, pa->npermits);
}

SYSCALL_1(dispose, int od)
{
    return Process_DisposeUResource(proc, pa->od);
}

SYSCALL_2(alloc_address_space, size_t nbytes, void * _Nullable * _Nonnull pOutMem)
{
    if (pa->nbytes > SSIZE_MAX) {
        return E2BIG;
    }

    return Process_AllocateAddressSpace(proc, __SSizeByClampingSize(pa->nbytes),
        pa->pOutMem);
}

SYSCALL_1(exit, int status)
{
    // Trigger the termination of the process. Note that the actual termination
    // is done asynchronously. That's why we sleep below since we don't want to
    // return to user space anymore.
    Process_Terminate(proc, pa->status);


    // This wait here will eventually be aborted when the dispatch queue that
    // owns this VP is terminated. This interrupt will be caused by the abort
    // of the call-as-user and thus this system call will not return to user
    // space anymore. Instead it will return to the dispatch queue main loop.
    VirtualProcessor_Sleep(kTimeInterval_Infinity);
    return EOK;
}

SYSCALL_4(spawn_process, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const spawn_opts_t* _Nonnull options, pid_t* _Nullable pOutPid)
{
    return Process_SpawnChildProcess(proc, pa->path, pa->argv, pa->options, pa->pOutPid);
}

SYSCALL_0(getpid)
{
    return Process_GetId(proc);
}

SYSCALL_0(getppid)
{
    return Process_GetParentId(proc);
}

SYSCALL_0(getuid)
{
    return Process_GetRealUserId(proc);
}

SYSCALL_0(getgid)
{
    return Process_GetRealGroupId(proc);
}


SYSCALL_0(getpargs)
{
    return (intptr_t) Process_GetArgumentsBaseAddress(proc);
}

SYSCALL_2(waitpid, pid_t pid, pstatus_t* _Nullable pOutStatus)
{
    return Process_WaitForTerminationOfChild(proc, pa->pid, pa->pOutStatus);
}

SYSCALL_4(mount, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
{
    return Process_Mount(proc, pa->objectType, pa->objectName, pa->atDirPath, pa->params);
}

SYSCALL_2(unmount, const char* _Nonnull atDirPath, UnmountOptions options)
{
    return Process_Unmount(proc, pa->atDirPath, pa->options);
}

SYSCALL_0(sync)
{
    FilesystemManager_Sync(gFilesystemManager);
    return EOK;
}

SYSCALL_0(coninit)
{
    extern errno_t SwitchToFullConsole(void);

    return SwitchToFullConsole();
}

SYSCALL_3(fsgetdisk, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    return Process_GetFilesystemDiskPath(proc, pa->fsid, pa->buf, pa->bufSize);
}


static syscall_t gSystemCallTable[SYSCALL_COUNT] = {
    REF_SYSCALL(read),
    REF_SYSCALL(write),
    REF_SYSCALL(clock_wait),
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
    REF_SYSCALL(getfinfo),
    REF_SYSCALL(opendir),
    REF_SYSCALL(setfinfo),
    REF_SYSCALL(access),
    REF_SYSCALL(fgetfinfo),
    REF_SYSCALL(fsetfinfo),
    REF_SYSCALL(unlink),
    REF_SYSCALL(rename),
    REF_SYSCALL(ioctl),
    REF_SYSCALL(truncate),
    REF_SYSCALL(ftruncate),
    REF_SYSCALL(mkfile),
    REF_SYSCALL(mkpipe),
    REF_SYSCALL(dispatch_timer),
    REF_SYSCALL(dispatch_queue_create),
    REF_SYSCALL(dispatch_queue_current),
    REF_SYSCALL(dispose),
    REF_SYSCALL(clock_gettime),
    REF_SYSCALL(lock_create),
    REF_SYSCALL(lock_trylock),
    REF_SYSCALL(lock_lock),
    REF_SYSCALL(lock_unlock),
    REF_SYSCALL(sem_create),
    REF_SYSCALL(sem_post),
    REF_SYSCALL(sem_wait),
    REF_SYSCALL(sem_trywait),
    REF_SYSCALL(cond_create),
    REF_SYSCALL(cond_wake),
    REF_SYSCALL(cond_timedwait),
    REF_SYSCALL(dispatch_remove_by_tag),
    REF_SYSCALL(mount),
    REF_SYSCALL(unmount),
    REF_SYSCALL(getgid),
    REF_SYSCALL(sync),
    REF_SYSCALL(coninit),
    REF_SYSCALL(fsgetdisk),
};
