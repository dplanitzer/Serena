//
//  syscalls.c
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
#include <time.h>
#include <kern/limits.h>
#include <kern/timespec.h>
#include <kpi/disk.h>
#include <kpi/fs.h>
#include <kpi/uid.h>


typedef intptr_t (*syscall_func_t)(void* _Nonnull p, void* _Nonnull);

typedef struct syscall {
    syscall_func_t  f;
    intptr_t        flags;
} syscall_t;

#define SC_ERRNO    1   /* System call returns an error that should be stored in vcpu->errno */
#define SC_VCPU     2   /* System call expects a vcpu_t* rather than a proc_t* */

#define SYSCALL_COUNT   58
static const syscall_t gSystemCallTable[SYSCALL_COUNT];


#define SYSCALL_ENTRY(__name, __flags) \
{(syscall_func_t)_SYSCALL_##__name, __flags}


#define SYSCALL_0(__name) \
struct args##__name { \
    unsigned int scno; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_1(__name, __p1) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_2(__name, __p1, __p2) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_3(__name, __p1, __p2, __p3) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_4(__name, __p1, __p2, __p3, __p4) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args_##__name* _Nonnull pa)

#define SYSCALL_5(__name, __p1, __p2, __p3, __p4, __p5) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
    __p5; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args_##__name* _Nonnull pa)

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
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args_##__name* _Nonnull pa)


////////////////////////////////////////////////////////////////////////////////

intptr_t _syscall_handler(VirtualProcessor* _Nonnull vcpu, unsigned int* _Nonnull args)
{
    ProcessRef curProc = DispatchQueue_GetOwningProcess(vcpu->dispatchQueue);
    const unsigned int nscno = sizeof(gSystemCallTable) / sizeof(syscall_t);
    const unsigned int scno = *args;
    intptr_t r;
    bool hasErrno;

    if (scno < nscno) {
        const syscall_t* sc = &gSystemCallTable[scno];
        void* p = ((sc->flags & SC_VCPU) == SC_VCPU) ? (void*)vcpu : (void*)curProc;

        r = sc->f(p, args);
        hasErrno = (sc->flags & SC_ERRNO) == SC_ERRNO;
    }
    else {
        r = ENOSYS;
        hasErrno = true;
    }

    if (hasErrno && r != 0) {
        vcpu->errno = (errno_t)r;
    }

    return r;
}


SYSCALL_4(mkfile, const char* _Nonnull path, unsigned int mode, uint32_t permissions, int* _Nonnull pOutIoc)
{
    return Process_CreateFile((ProcessRef)p, pa->path, pa->mode, (FilePermissions)pa->permissions, pa->pOutIoc);
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

SYSCALL_1(close, int ioc)
{
    return Process_CloseChannel((ProcessRef)p, pa->ioc);
}

SYSCALL_4(read, int ioc, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead)
{
    return Process_ReadChannel((ProcessRef)p, pa->ioc, pa->buffer, pa->nBytesToRead, pa->nBytesRead);
}

SYSCALL_4(write, int ioc, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten)
{
    return Process_WriteChannel((ProcessRef)p, pa->ioc, pa->buffer, pa->nBytesToWrite, pa->nBytesWritten);
}

SYSCALL_4(seek, int ioc, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    return Process_SeekChannel((ProcessRef)p, pa->ioc, pa->offset, pa->pOutOldPosition, pa->whence);
}

SYSCALL_2(mkdir, const char* _Nonnull path, uint32_t mode)
{
    return Process_CreateDirectory((ProcessRef)p, pa->path, (FilePermissions) pa->mode);
}

SYSCALL_2(getcwd, char* _Nonnull buffer, size_t bufferSize)
{
    return Process_GetWorkingDirectoryPath((ProcessRef)p, pa->buffer, pa->bufferSize);
}

SYSCALL_1(chdir, const char* _Nonnull path)
{
    return Process_SetWorkingDirectoryPath((ProcessRef)p, pa->path);
}

SYSCALL_2(getfinfo, const char* _Nonnull path, finfo_t* _Nonnull pOutInfo)
{
    return Process_GetFileInfo((ProcessRef)p, pa->path, pa->pOutInfo);
}

SYSCALL_2(setfinfo, const char* _Nonnull path, fmutinfo_t* _Nonnull pInfo)
{
    return Process_SetFileInfo((ProcessRef)p, pa->path, pa->pInfo);
}

SYSCALL_2(fgetfinfo, int ioc, finfo_t* _Nonnull pOutInfo)
{
    return Process_GetFileInfo_ioc((ProcessRef)p, pa->ioc, pa->pOutInfo);
}

SYSCALL_2(fsetfinfo, int ioc, fmutinfo_t* _Nonnull pInfo)
{
    return Process_SetFileInfo_ioc((ProcessRef)p, pa->ioc, pa->pInfo);
}

SYSCALL_2(truncate, const char* _Nonnull path, off_t length)
{
    return Process_TruncateFile((ProcessRef)p, pa->path, pa->length);
}

SYSCALL_2(ftruncate, int ioc, off_t length)
{
    return Process_TruncateFile_ioc((ProcessRef)p, pa->ioc, pa->length);
}

SYSCALL_3(ioctl, int ioc, int cmd, va_list _Nullable ap)
{
    return Process_Iocall((ProcessRef)p, pa->ioc, pa->cmd, pa->ap);
}

SYSCALL_2(access, const char* _Nonnull path, uint32_t mode)
{
    return Process_CheckAccess((ProcessRef)p, pa->path, pa->mode);
}

SYSCALL_1(unlink, const char* _Nonnull path)
{
    return Process_Unlink((ProcessRef)p, pa->path);
}

SYSCALL_2(rename, const char* _Nonnull oldPath, const char* _Nonnull newPath)
{
    return Process_Rename((ProcessRef)p, pa->oldPath, pa->newPath);
}


SYSCALL_0(getumask)
{
    return Process_GetFileCreationMask((ProcessRef)p);
}

SYSCALL_1(setumask, uint32_t mask)
{
    Process_SetFileCreationMask((ProcessRef)p, pa->mask);
    return EOK;
}

SYSCALL_2(clock_wait, int clock, const struct timespec* _Nonnull delay)
{
    if (pa->delay->tv_nsec < 0 || pa->delay->tv_nsec >= ONE_SECOND_IN_NANOS) {
        return EINVAL;
    }
    if (pa->clock != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    return VirtualProcessor_Sleep(*(pa->delay));
}

SYSCALL_2(clock_gettime, int clock, struct timespec* _Nonnull time)
{
    if (pa->clock != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    *(pa->time) = MonotonicClock_GetCurrentTime();
    return EOK;
}

SYSCALL_5(dispatch, int od, const VoidFunc_2 _Nonnull func, void* _Nullable ctx, uint32_t options, uintptr_t tag)
{
    return Process_DispatchUserClosure((ProcessRef)p, pa->od, pa->func, pa->ctx, pa->options, pa->tag);
}

SYSCALL_6(dispatch_timer, int od, struct timespec deadline, struct timespec interval, const VoidFunc_1 _Nonnull func, void* _Nullable ctx, uintptr_t tag)
{
    return Process_DispatchUserTimer((ProcessRef)p, pa->od, pa->deadline, pa->interval, pa->func, pa->ctx, pa->tag);
}

SYSCALL_5(dispatch_queue_create, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
{
    return Process_CreateDispatchQueue((ProcessRef)p, pa->minConcurrency, pa->maxConcurrency, pa->qos, pa->priority, pa->pOutQueue);
}

SYSCALL_2(dispatch_remove_by_tag, int od, uintptr_t tag)
{
    return Process_DispatchRemoveByTag((ProcessRef)p, pa->od, pa->tag);
}

SYSCALL_0(dispatch_queue_current)
{
    return Process_GetCurrentDispatchQueue((ProcessRef)p);
}


SYSCALL_1(cond_create, int* _Nonnull pOutOd)
{
    return Process_CreateUConditionVariable((ProcessRef)p, pa->pOutOd);
}

SYSCALL_3(cond_wake, int od, int dlock, unsigned int options)
{
    return Process_WakeUConditionVariable((ProcessRef)p, pa->od, pa->dlock, pa->options);
}

SYSCALL_3(cond_timedwait, int od, int dlock, const struct timespec* _Nullable deadline)
{
    const struct timespec ts = (pa->deadline) ? *(pa->deadline) : TIMESPEC_INF;

    return Process_WaitUConditionVariable((ProcessRef)p, pa->od, pa->dlock, ts);
}


SYSCALL_1(lock_create, int* _Nonnull pOutOd)
{
    return Process_CreateULock((ProcessRef)p, pa->pOutOd);
}

SYSCALL_1(lock_trylock, int od)
{
    return Process_TryULock((ProcessRef)p, pa->od);
}

SYSCALL_1(lock_lock, int od)
{
    return Process_LockULock((ProcessRef)p, pa->od);
}

SYSCALL_1(lock_unlock, int od)
{
    return Process_UnlockULock((ProcessRef)p, pa->od);
}


SYSCALL_2(sem_create, int npermits, int* _Nonnull pOutOd)
{
    return Process_CreateUSemaphore((ProcessRef)p, pa->npermits, pa->pOutOd);
}

SYSCALL_2(sem_post, int od, int npermits)
{
    return Process_RelinquishUSemaphore((ProcessRef)p, pa->od, pa->npermits);
}

SYSCALL_3(sem_wait, int od, int npermits, struct timespec deadline)
{
    return Process_AcquireUSemaphore((ProcessRef)p, pa->od, pa->npermits, pa->deadline);
}

SYSCALL_2(sem_trywait, int od, int npermits)
{
    return Process_TryAcquireUSemaphore((ProcessRef)p, pa->od, pa->npermits);
}

SYSCALL_1(dispose, int od)
{
    return Process_DisposeUResource((ProcessRef)p, pa->od);
}

SYSCALL_2(alloc_address_space, size_t nbytes, void * _Nullable * _Nonnull pOutMem)
{
    if (pa->nbytes > SSIZE_MAX) {
        return E2BIG;
    }

    return Process_AllocateAddressSpace((ProcessRef)p, __SSizeByClampingSize(pa->nbytes),
        pa->pOutMem);
}

SYSCALL_1(exit, int status)
{
    // Trigger the termination of the process. Note that the actual termination
    // is done asynchronously. That's why we sleep below since we don't want to
    // return to user space anymore.
    Process_Terminate((ProcessRef)p, pa->status);


    // This wait here will eventually be aborted when the dispatch queue that
    // owns this VP is terminated. This interrupt will be caused by the abort
    // of the call-as-user and thus this system call will not return to user
    // space anymore. Instead it will return to the dispatch queue main loop.
    VirtualProcessor_Sleep(TIMESPEC_INF);
    return 0;
}

SYSCALL_4(spawn_process, const char* _Nonnull path, const char* _Nullable * _Nullable argv, const spawn_opts_t* _Nonnull options, pid_t* _Nullable pOutPid)
{
    return Process_SpawnChildProcess((ProcessRef)p, pa->path, pa->argv, pa->options, pa->pOutPid);
}

SYSCALL_0(getpid)
{
    return Process_GetId((ProcessRef)p);
}

SYSCALL_0(getppid)
{
    return Process_GetParentId((ProcessRef)p);
}

SYSCALL_0(getuid)
{
    return Process_GetRealUserId((ProcessRef)p);
}

SYSCALL_0(getgid)
{
    return Process_GetRealGroupId((ProcessRef)p);
}


SYSCALL_0(getpargs)
{
    return (intptr_t) Process_GetArgumentsBaseAddress((ProcessRef)p);
}

SYSCALL_2(waitpid, pid_t pid, pstatus_t* _Nullable pOutStatus)
{
    return Process_WaitForTerminationOfChild((ProcessRef)p, pa->pid, pa->pOutStatus);
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

SYSCALL_0(coninit)
{
    extern errno_t SwitchToFullConsole(void);

    return SwitchToFullConsole();
}

SYSCALL_3(fsgetdisk, fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    return Process_GetFilesystemDiskPath((ProcessRef)p, pa->fsid, pa->buf, pa->bufSize);
}

SYSCALL_0(vcpuerr)
{
    return (intptr_t)&(((VirtualProcessor*)p)->errno);
}

SYSCALL_3(chown, const char* _Nonnull path, uid_t uid, gid_t gid)
{
    return Process_SetFileOwner((ProcessRef)p, pa->path, pa->uid, pa->gid);
}


static const syscall_t gSystemCallTable[SYSCALL_COUNT] = {
    SYSCALL_ENTRY(read, SC_ERRNO),
    SYSCALL_ENTRY(write, SC_ERRNO),
    SYSCALL_ENTRY(clock_wait, SC_ERRNO),
    SYSCALL_ENTRY(dispatch, SC_ERRNO),
    SYSCALL_ENTRY(alloc_address_space, SC_ERRNO),
    SYSCALL_ENTRY(exit, 0),
    SYSCALL_ENTRY(spawn_process, SC_ERRNO),
    SYSCALL_ENTRY(getpid, 0),
    SYSCALL_ENTRY(getppid, 0),
    SYSCALL_ENTRY(getpargs, 0),
    SYSCALL_ENTRY(open, SC_ERRNO),
    SYSCALL_ENTRY(close, SC_ERRNO),
    SYSCALL_ENTRY(waitpid, SC_ERRNO),
    SYSCALL_ENTRY(seek, SC_ERRNO),
    SYSCALL_ENTRY(getcwd, SC_ERRNO),
    SYSCALL_ENTRY(chdir, SC_ERRNO),
    SYSCALL_ENTRY(getuid, 0),
    SYSCALL_ENTRY(getumask, 0),
    SYSCALL_ENTRY(setumask, 0),
    SYSCALL_ENTRY(mkdir, SC_ERRNO),
    SYSCALL_ENTRY(getfinfo, SC_ERRNO),
    SYSCALL_ENTRY(opendir, SC_ERRNO),
    SYSCALL_ENTRY(setfinfo, SC_ERRNO),
    SYSCALL_ENTRY(access, SC_ERRNO),
    SYSCALL_ENTRY(fgetfinfo, SC_ERRNO),
    SYSCALL_ENTRY(fsetfinfo, SC_ERRNO),
    SYSCALL_ENTRY(unlink, SC_ERRNO),
    SYSCALL_ENTRY(rename, SC_ERRNO),
    SYSCALL_ENTRY(ioctl, SC_ERRNO),
    SYSCALL_ENTRY(truncate, SC_ERRNO),
    SYSCALL_ENTRY(ftruncate, SC_ERRNO),
    SYSCALL_ENTRY(mkfile, SC_ERRNO),
    SYSCALL_ENTRY(mkpipe, SC_ERRNO),
    SYSCALL_ENTRY(dispatch_timer, SC_ERRNO),
    SYSCALL_ENTRY(dispatch_queue_create, SC_ERRNO),
    SYSCALL_ENTRY(dispatch_queue_current, 0),
    SYSCALL_ENTRY(dispose, SC_ERRNO),
    SYSCALL_ENTRY(clock_gettime, SC_ERRNO),
    SYSCALL_ENTRY(lock_create, SC_ERRNO),
    SYSCALL_ENTRY(lock_trylock, SC_ERRNO),
    SYSCALL_ENTRY(lock_lock, SC_ERRNO),
    SYSCALL_ENTRY(lock_unlock, SC_ERRNO),
    SYSCALL_ENTRY(sem_create, SC_ERRNO),
    SYSCALL_ENTRY(sem_post, SC_ERRNO),
    SYSCALL_ENTRY(sem_wait, SC_ERRNO),
    SYSCALL_ENTRY(sem_trywait, SC_ERRNO),
    SYSCALL_ENTRY(cond_create, SC_ERRNO),
    SYSCALL_ENTRY(cond_wake, SC_ERRNO),
    SYSCALL_ENTRY(cond_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(dispatch_remove_by_tag, SC_ERRNO),
    SYSCALL_ENTRY(mount, SC_ERRNO),
    SYSCALL_ENTRY(unmount, SC_ERRNO),
    SYSCALL_ENTRY(getgid, 0),
    SYSCALL_ENTRY(sync, SC_ERRNO),
    SYSCALL_ENTRY(coninit, SC_ERRNO),
    SYSCALL_ENTRY(fsgetdisk, SC_ERRNO),
    SYSCALL_ENTRY(vcpuerr, SC_VCPU),
    SYSCALL_ENTRY(chown, SC_ERRNO)
};
