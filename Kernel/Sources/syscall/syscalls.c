//
//  syscalls.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/wait.h>


SYSCALL_REF(coninit);

SYSCALL_REF(clock_gettime);
SYSCALL_REF(clock_nanosleep);
SYSCALL_REF(clock_getres);

SYSCALL_REF(excpt_sethandler);

SYSCALL_REF(close);
SYSCALL_REF(read);
SYSCALL_REF(write);
SYSCALL_REF(seek);
SYSCALL_REF(fcntl);
SYSCALL_REF(ioctl);

SYSCALL_REF(mkfile);
SYSCALL_REF(open);
SYSCALL_REF(opendir);
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
SYSCALL_REF(chown);
SYSCALL_REF(chmod);
SYSCALL_REF(utimens);

SYSCALL_REF(mkpipe);

SYSCALL_REF(mount);
SYSCALL_REF(unmount);
SYSCALL_REF(sync);
SYSCALL_REF(fsgetdisk);

SYSCALL_REF(exit);
SYSCALL_REF(spawn_process);
SYSCALL_REF(getpid);
SYSCALL_REF(getppid);
SYSCALL_REF(getpgrp);
SYSCALL_REF(getsid);
SYSCALL_REF(getuid);
SYSCALL_REF(getgid);
SYSCALL_REF(getpargs);
SYSCALL_REF(proc_timedjoin);
SYSCALL_REF(proc_exec);

SYSCALL_REF(alloc_address_space);

SYSCALL_REF(sigwait);
SYSCALL_REF(sigtimedwait);
SYSCALL_REF(sigpending);
SYSCALL_REF(sigsend);
SYSCALL_REF(sigroute);
SYSCALL_REF(sigurgent);

SYSCALL_REF(wq_create);
SYSCALL_REF(wq_dispose);
SYSCALL_REF(wq_wait);
SYSCALL_REF(wq_timedwait);
SYSCALL_REF(wq_wakeup);
SYSCALL_REF(wq_wakeup_then_timedwait);

SYSCALL_REF(vcpu_errno);
SYSCALL_REF(vcpu_getid);
SYSCALL_REF(vcpu_getgrp);
SYSCALL_REF(vcpu_getdata);
SYSCALL_REF(vcpu_setdata);
SYSCALL_REF(vcpu_acquire);
SYSCALL_REF(vcpu_relinquish_self);
SYSCALL_REF(vcpu_suspend);
SYSCALL_REF(vcpu_resume);
SYSCALL_REF(vcpu_yield);
SYSCALL_REF(vcpu_getschedparams);
SYSCALL_REF(vcpu_setschedparams);


////////////////////////////////////////////////////////////////////////////////

#define SYSCALL_COUNT   69

static const syscall_t gSystemCallTable[SYSCALL_COUNT] = {
    SYSCALL_ENTRY(read, SC_ERRNO),
    SYSCALL_ENTRY(write, SC_ERRNO),
    SYSCALL_ENTRY(clock_nanosleep, SC_ERRNO),
    SYSCALL_ENTRY(alloc_address_space, SC_ERRNO),
    SYSCALL_ENTRY(exit, 0),
    SYSCALL_ENTRY(spawn_process, SC_ERRNO),
    SYSCALL_ENTRY(getpid, 0),
    SYSCALL_ENTRY(getppid, 0),
    SYSCALL_ENTRY(getpargs, 0),
    SYSCALL_ENTRY(open, SC_ERRNO),
    SYSCALL_ENTRY(close, SC_ERRNO),
    SYSCALL_ENTRY(proc_timedjoin, SC_ERRNO),
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
    SYSCALL_ENTRY(wq_dispose, SC_ERRNO),
    SYSCALL_ENTRY(clock_gettime, SC_ERRNO),
    SYSCALL_ENTRY(mount, SC_ERRNO),
    SYSCALL_ENTRY(unmount, SC_ERRNO),
    SYSCALL_ENTRY(getgid, 0),
    SYSCALL_ENTRY(sync, SC_ERRNO),
    SYSCALL_ENTRY(coninit, SC_ERRNO),
    SYSCALL_ENTRY(fsgetdisk, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_errno, 0),
    SYSCALL_ENTRY(chown, SC_ERRNO),
    SYSCALL_ENTRY(fcntl, SC_ERRNO),
    SYSCALL_ENTRY(chmod, SC_ERRNO),
    SYSCALL_ENTRY(utimens, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_yield, 0),
    SYSCALL_ENTRY(wq_create, SC_ERRNO),
    SYSCALL_ENTRY(wq_wait, SC_ERRNO),
    SYSCALL_ENTRY(wq_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_wakeup, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_getid, 0),
    SYSCALL_ENTRY(sigroute, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_getdata, 0),
    SYSCALL_ENTRY(vcpu_setdata, 0),
    SYSCALL_ENTRY(sigwait, SC_ERRNO),
    SYSCALL_ENTRY(sigtimedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_wakeup_then_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(sigpending, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_getgrp, 0),
    SYSCALL_ENTRY(getpgrp, 0),
    SYSCALL_ENTRY(getsid, 0),
    SYSCALL_ENTRY(vcpu_acquire, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_relinquish_self, 0),
    SYSCALL_ENTRY(vcpu_suspend, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_resume, SC_ERRNO),
    SYSCALL_ENTRY(sigsend, SC_ERRNO),
    SYSCALL_ENTRY(sigurgent, 0),
    SYSCALL_ENTRY(excpt_sethandler, SC_ERRNO),
    SYSCALL_ENTRY(proc_exec, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_getschedparams, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_setschedparams, SC_ERRNO),
    SYSCALL_ENTRY(clock_getres, SC_ERRNO),
};

////////////////////////////////////////////////////////////////////////////////


static void _handle_urgent_signals(vcpu_t _Nonnull vp)
{
    const sigset_t sigs = vp->pending_sigs & SIGSET_URGENTS;
    vp->pending_sigs &= ~SIGSET_URGENTS;

    if ((sigs & _SIGBIT(SIGKILL)) != 0) {
        Process_Exit(vp->proc, JREASON_SIGNAL, SIGKILL);
        /* NOT REACHED */
    }

    if ((sigs & _SIGBIT(SIGSUSPEND)) != 0) {
        vcpu_suspend(vp);
    }
}

intptr_t _syscall_handler(vcpu_t _Nonnull vp, unsigned int* _Nonnull args)
{
    const unsigned int scno = *args;
    intptr_t r;
    bool hasErrno;
    
    if (scno < SYSCALL_COUNT) {
        const syscall_t* sc = &gSystemCallTable[scno];

        r = sc->f(vp, args);
        hasErrno = (sc->flags & SC_ERRNO) == SC_ERRNO;
    }
    else {
        r = ENOSYS;
        hasErrno = true;
    }


    if ((vp->pending_sigs & SIGSET_URGENTS) != 0) {
        _handle_urgent_signals(vp);
    }


    if (hasErrno) {
        if (r != 0) {
            vp->uerrno = (errno_t)r;
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
