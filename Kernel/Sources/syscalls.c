//
//  syscalls.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <dispatcher/delay.h>
#include <dispatcher/VirtualProcessor.h>
#include <dispatcher/WaitQueue.h>
#include <dispatchqueue/DispatchQueue.h>
#include <filemanager/FilesystemManager.h>
#include <filesystem/IOChannel.h>
#include <hal/MonotonicClock.h>
#include <process/ProcessPriv.h>
#include <time.h>
#include <kern/limits.h>
#include <kern/timespec.h>
#include <kpi/disk.h>
#include <kpi/fs.h>
#include <kpi/signal.h>
#include <kpi/uid.h>


typedef intptr_t (*syscall_func_t)(void* _Nonnull p, void* _Nonnull);

typedef struct syscall {
    syscall_func_t  f;
    intptr_t        flags;
} syscall_t;

#define SC_ERRNO    1   /* System call returns an error that should be stored in vcpu->errno */
#define SC_VCPU     2   /* System call expects a vcpu_t* rather than a proc_t* */

#define SYSCALL_COUNT   59
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

    if (hasErrno) {
        if (r != 0) {
            vcpu->uerrno = (errno_t)r;
            return -1;
        }
        else {
            return 0;
        }
    }
    else {
        return r;
    }
}


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

SYSCALL_4(fcntl, int fd, int cmd, int* _Nonnull pResult, va_list _Nullable ap)
{
    return Process_Fcntl((ProcessRef)p, pa->fd, pa->cmd, pa->pResult, pa->ap);
}

SYSCALL_3(ioctl, int fd, int cmd, va_list _Nullable ap)
{
    return Process_Iocall((ProcessRef)p, pa->fd, pa->cmd, pa->ap);
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

SYSCALL_4(clock_nanosleep, int clock, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
{
    if (!timespec_isvalid(pa->wtp)) {
        return EINVAL;
    }
    if (pa->clock != CLOCK_MONOTONIC) {
        return ENODEV;
    }


    int options = 0;
    if ((pa->flags & TIMER_ABSTIME) == TIMER_ABSTIME) {
        options |= WAIT_ABSTIME;
    }


    // This is a medium or long wait -> context switch away
    return _sleep(&((ProcessRef)p)->sleepQueue, NULL, options, pa->wtp, pa->rmtp);
}

SYSCALL_2(clock_gettime, int clock, struct timespec* _Nonnull time)
{
    if (pa->clock != CLOCK_MONOTONIC) {
        return ENODEV;
    }

    MonotonicClock_GetCurrentTime(pa->time);
    return EOK;
}

SYSCALL_5(dispatch, int od, const VoidFunc_2 _Nonnull func, void* _Nullable ctx, uint32_t options, uintptr_t tag)
{
    return Process_DispatchUserClosure((ProcessRef)p, pa->od, pa->func, pa->ctx, pa->options, pa->tag);
}

SYSCALL_6(dispatch_timer, int od, const struct timespec* _Nonnull deadline, const struct timespec* _Nonnull interval, const VoidFunc_1 _Nonnull func, void* _Nullable ctx, uintptr_t tag)
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

// XXX Will be removed when we'll do the process termination algorithm
static WaitQueue gHackQueue;

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
    _sleep(&gHackQueue, NULL, WAIT_ABSTIME, &TIMESPEC_INF, NULL);
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

SYSCALL_3(waitpid, pid_t pid, struct _pstatus* _Nonnull pOutStatus, int options)
{
    return Process_WaitForTerminationOfChild((ProcessRef)p, pa->pid, pa->pOutStatus, pa->options);
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

SYSCALL_0(vcpu_errno)
{
    return (intptr_t)&(((VirtualProcessor*)p)->uerrno);
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

SYSCALL_0(sched_yield)
{
    VirtualProcessor_Yield();
    return EOK;
}

SYSCALL_2(wq_create, int policy, int* _Nonnull pOutOd)
{
    return Process_CreateUWaitQueue((ProcessRef)p, pa->policy, pa->pOutOd);
}

SYSCALL_2(wq_wait, int q, const sigset_t* _Nullable mask)
{
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_Wait_UWaitQueue(pp, pa->q, pa->mask);
}

SYSCALL_4(wq_timedwait, int q, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_TimedWait_UWaitQueue(pp, pa->q, pa->mask, pa->flags, pa->wtp);
}

SYSCALL_5(wq_timedwakewait, int q, int oq, const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp)
{
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_TimedWakeWait_UWaitQueue(pp, pa->q, pa->oq, pa->mask, pa->flags, pa->wtp);
}

SYSCALL_2(wq_wakeup, int q, int flags)
{
    return Process_Wakeup_UWaitQueue((ProcessRef)p, pa->q, pa->flags);
}

SYSCALL_2(sigwait, const sigset_t* _Nullable mask, const sigset_t* _Nonnull set)
{
#if 0
    ProcessRef pp = (ProcessRef)p;
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_SigWait_UWaitQueue((ProcessRef)p, pa->od, pmask, pa->osigs);
#else
    return 0;
#endif
}

SYSCALL_4(sigtimedwait, const sigset_t* _Nullable mask, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp)
{
#if 0
    const sigset_t newMask = (pa->mask) ? *(pa->mask) & ~SIGSET_NONMASKABLES : 0;
    const sigset_t* pmask = (pa->mask) ? &newMask : NULL;

    return Process_SigTimedWait_UWaitQueue((ProcessRef)p, pa->od, pmask, pa->osigs, pa->flags, pa->wtp);
#else
    return 0;
#endif
}

SYSCALL_0(vcpu_self)
{
    return (intptr_t)((VirtualProcessor*)p)->vpid;
}

SYSCALL_3(vcpu_setsigmask, int op, sigset_t mask, sigset_t* _Nullable oldmask)
{
    return VirtualProcessor_SetSignalMask((VirtualProcessor*)p, pa->op, pa->mask & ~SIGSET_NONMASKABLES, pa->oldmask);
}

SYSCALL_0(vcpu_getdata)
{
    return ((VirtualProcessor*)p)->udata;
}

SYSCALL_1(vcpu_setdata, intptr_t data)
{
    ((VirtualProcessor*)p)->udata = pa->data;
    return EOK;
}


static const syscall_t gSystemCallTable[SYSCALL_COUNT] = {
    SYSCALL_ENTRY(read, SC_ERRNO),
    SYSCALL_ENTRY(write, SC_ERRNO),
    SYSCALL_ENTRY(clock_nanosleep, SC_ERRNO),
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
    SYSCALL_ENTRY(umask, 0),
    SYSCALL_ENTRY(mkdir, SC_ERRNO),
    SYSCALL_ENTRY(stat, SC_ERRNO),
    SYSCALL_ENTRY(opendir, SC_ERRNO),
    SYSCALL_ENTRY(access, SC_ERRNO),
    SYSCALL_ENTRY(fstat, SC_ERRNO),
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
    SYSCALL_ENTRY(dispatch_remove_by_tag, SC_ERRNO),
    SYSCALL_ENTRY(mount, SC_ERRNO),
    SYSCALL_ENTRY(unmount, SC_ERRNO),
    SYSCALL_ENTRY(getgid, 0),
    SYSCALL_ENTRY(sync, SC_ERRNO),
    SYSCALL_ENTRY(coninit, SC_ERRNO),
    SYSCALL_ENTRY(fsgetdisk, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_errno, SC_VCPU),
    SYSCALL_ENTRY(chown, SC_ERRNO),
    SYSCALL_ENTRY(fcntl, SC_ERRNO),
    SYSCALL_ENTRY(chmod, SC_ERRNO),
    SYSCALL_ENTRY(utimens, SC_ERRNO),
    SYSCALL_ENTRY(sched_yield, 0),
    SYSCALL_ENTRY(wq_create, SC_ERRNO),
    SYSCALL_ENTRY(wq_wait, SC_ERRNO),
    SYSCALL_ENTRY(wq_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_wakeup, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_self, SC_VCPU),
    SYSCALL_ENTRY(vcpu_setsigmask, SC_VCPU|SC_ERRNO),
    SYSCALL_ENTRY(vcpu_getdata, SC_VCPU),
    SYSCALL_ENTRY(vcpu_setdata, SC_VCPU),
    SYSCALL_ENTRY(sigwait, SC_ERRNO),
    SYSCALL_ENTRY(sigtimedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_timedwakewait, SC_ERRNO),
};
