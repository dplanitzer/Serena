//
//  syscalls.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"


#define SYSCALL_COUNT   59
static const syscall_t gSystemCallTable[SYSCALL_COUNT];


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


SYSCALL_REF(dispose);
SYSCALL_REF(coninit);

SYSCALL_REF(clock_gettime);
SYSCALL_REF(clock_nanosleep);

SYSCALL_REF(dispatch);
SYSCALL_REF(dispatch_timer);
SYSCALL_REF(dispatch_queue_create);
SYSCALL_REF(dispatch_remove_by_tag);
SYSCALL_REF(dispatch_queue_current);

SYSCALL_REF(close);
SYSCALL_REF(read);
SYSCALL_REF(write);
SYSCALL_REF(seek);
SYSCALL_REF(fcntl);
SYSCALL_REF(ioctl);

SYSCALL_REF(mkfile);
SYSCALL_REF(open);
SYSCALL_REF(opendir);
SYSCALL_REF(mkpipe);
SYSCALL_REF(mkdir);
SYSCALL_REF(getcwd);
SYSCALL_REF(chdir);
SYSCALL_REF(stat);
SYSCALL_REF(fstat);
SYSCALL_REF(truncate);
SYSCALL_REF(ftruncate);
SYSCALL_REF(access);
SYSCALL_REF(unlink);
SYSCALL_REF(rename);
SYSCALL_REF(umask);
SYSCALL_REF(mount);
SYSCALL_REF(unmount);
SYSCALL_REF(sync);
SYSCALL_REF(fsgetdisk);
SYSCALL_REF(chown);
SYSCALL_REF(chmod);
SYSCALL_REF(utimens);

SYSCALL_REF(exit);
SYSCALL_REF(spawn_process);
SYSCALL_REF(getpid);
SYSCALL_REF(getppid);
SYSCALL_REF(getuid);
SYSCALL_REF(getgid);
SYSCALL_REF(getpargs);
SYSCALL_REF(waitpid);

SYSCALL_REF(alloc_address_space);

SYSCALL_REF(sigwait);
SYSCALL_REF(sigtimedwait);

SYSCALL_REF(wq_create);
SYSCALL_REF(wq_wait);
SYSCALL_REF(wq_timedwait);
SYSCALL_REF(wq_wakeup);
SYSCALL_REF(wq_timedwakewait);

SYSCALL_REF(sched_yield);

SYSCALL_REF(vcpu_errno);
SYSCALL_REF(vcpu_self);
SYSCALL_REF(vcpu_setsigmask);
SYSCALL_REF(vcpu_getdata);
SYSCALL_REF(vcpu_setdata);


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
