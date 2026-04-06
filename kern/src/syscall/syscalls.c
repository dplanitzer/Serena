//
//  syscalls.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/process.h>


SYSCALL_REF(coninit);
SYSCALL_REF(nosys);
SYSCALL_REF(nullsys);

SYSCALL_REF(clock_gettime);
SYSCALL_REF(clock_nanosleep);
SYSCALL_REF(clock_getres);

SYSCALL_REF(excpt_sethandler);

SYSCALL_REF(host_info);
SYSCALL_REF(host_procs);
SYSCALL_REF(host_filesystems);

SYSCALL_REF(fs_info);
SYSCALL_REF(fs_diskpath);
SYSCALL_REF(fs_label);
SYSCALL_REF(fs_setlabel);
SYSCALL_REF(fs_mount);
SYSCALL_REF(fs_unmount);
SYSCALL_REF(fs_sync);

SYSCALL_REF(close);
SYSCALL_REF(read);
SYSCALL_REF(write);
SYSCALL_REF(seek);
SYSCALL_REF(fcntl);
SYSCALL_REF(ioctl);

SYSCALL_REF(mkfile);
SYSCALL_REF(open);
SYSCALL_REF(dir_open);
SYSCALL_REF(fs_create_directory);
SYSCALL_REF(proc_cwd);
SYSCALL_REF(proc_setcwd);
SYSCALL_REF(stat);
SYSCALL_REF(fstat);
SYSCALL_REF(fs_truncate);
SYSCALL_REF(ftruncate);
SYSCALL_REF(fs_access);
SYSCALL_REF(fs_unlink);
SYSCALL_REF(rename);
SYSCALL_REF(umask);
SYSCALL_REF(fs_setowner);
SYSCALL_REF(fs_setperms);
SYSCALL_REF(utimens);

SYSCALL_REF(mkpipe);

SYSCALL_REF(exit);
SYSCALL_REF(spawn_process);
SYSCALL_REF(getpargs);
SYSCALL_REF(proc_timedjoin);
SYSCALL_REF(proc_exec);
SYSCALL_REF(proc_schedparam);
SYSCALL_REF(proc_setschedparam);
SYSCALL_REF(proc_info);
SYSCALL_REF(proc_name);
SYSCALL_REF(proc_vcpus);


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
SYSCALL_REF(vcpu_getdata);
SYSCALL_REF(vcpu_setdata);
SYSCALL_REF(vcpu_acquire);
SYSCALL_REF(vcpu_relinquish_self);
SYSCALL_REF(vcpu_suspend);
SYSCALL_REF(vcpu_resume);
SYSCALL_REF(vcpu_yield);
SYSCALL_REF(vcpu_policy);
SYSCALL_REF(vcpu_setpolicy);
SYSCALL_REF(vcpu_state);
SYSCALL_REF(vcpu_setstate);
SYSCALL_REF(vcpu_info);


////////////////////////////////////////////////////////////////////////////////

#define SYSCALL_COUNT   76

static const syscall_entry_t g_syscall_table[SYSCALL_COUNT] = {
    SYSCALL_ENTRY(read, SC_ERRNO),
    SYSCALL_ENTRY(write, SC_ERRNO),
    SYSCALL_ENTRY(clock_nanosleep, SC_ERRNO),
    SYSCALL_ENTRY(alloc_address_space, SC_ERRNO),
    SYSCALL_ENTRY(exit, SC_NORETURN),
    SYSCALL_ENTRY(spawn_process, SC_ERRNO),
    SYSCALL_ENTRY(proc_name, SC_ERRNO),
    SYSCALL_ENTRY(proc_vcpus, SC_ERRNO),
    SYSCALL_ENTRY(getpargs, SC_PTR),
    SYSCALL_ENTRY(open, SC_ERRNO),
    SYSCALL_ENTRY(close, SC_ERRNO),
    SYSCALL_ENTRY(proc_timedjoin, SC_ERRNO),
    SYSCALL_ENTRY(seek, SC_ERRNO),
    SYSCALL_ENTRY(proc_cwd, SC_ERRNO),
    SYSCALL_ENTRY(proc_setcwd, SC_ERRNO),
    SYSCALL_ENTRY(host_procs, SC_ERRNO),
    SYSCALL_ENTRY(umask, SC_INT),
    SYSCALL_ENTRY(fs_create_directory, SC_ERRNO),
    SYSCALL_ENTRY(stat, SC_ERRNO),
    SYSCALL_ENTRY(dir_open, SC_ERRNO),
    SYSCALL_ENTRY(fs_access, SC_ERRNO),
    SYSCALL_ENTRY(fstat, SC_ERRNO),
    SYSCALL_ENTRY(fs_unlink, SC_ERRNO),
    SYSCALL_ENTRY(rename, SC_ERRNO),
    SYSCALL_ENTRY(ioctl, SC_ERRNO),
    SYSCALL_ENTRY(fs_truncate, SC_ERRNO),
    SYSCALL_ENTRY(ftruncate, SC_ERRNO),
    SYSCALL_ENTRY(mkfile, SC_ERRNO),
    SYSCALL_ENTRY(mkpipe, SC_ERRNO),
    SYSCALL_ENTRY(wq_dispose, SC_ERRNO),
    SYSCALL_ENTRY(clock_gettime, SC_ERRNO),
    SYSCALL_ENTRY(fs_mount, SC_ERRNO),
    SYSCALL_ENTRY(fs_unmount, SC_ERRNO),
    SYSCALL_ENTRY(host_info, SC_ERRNO),
    SYSCALL_ENTRY(fs_sync, SC_ERRNO),
    SYSCALL_ENTRY(coninit, SC_ERRNO),
    SYSCALL_ENTRY(fs_diskpath, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_errno, SC_INT),
    SYSCALL_ENTRY(fs_setowner, SC_ERRNO),
    SYSCALL_ENTRY(fcntl, SC_ERRNO),
    SYSCALL_ENTRY(fs_setperms, SC_ERRNO),
    SYSCALL_ENTRY(utimens, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_yield, SC_VOID),
    SYSCALL_ENTRY(wq_create, SC_ERRNO),
    SYSCALL_ENTRY(wq_wait, SC_ERRNO),
    SYSCALL_ENTRY(wq_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_wakeup, SC_ERRNO),
    SYSCALL_ENTRY(proc_info, SC_ERRNO),
    SYSCALL_ENTRY(sigroute, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_getdata, SC_PTR),
    SYSCALL_ENTRY(vcpu_setdata, SC_INT),
    SYSCALL_ENTRY(sigwait, SC_ERRNO),
    SYSCALL_ENTRY(sigtimedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_wakeup_then_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(sigpending, SC_ERRNO),
    SYSCALL_ENTRY(host_filesystems, SC_ERRNO),
    SYSCALL_ENTRY(fs_info, SC_ERRNO),
    SYSCALL_ENTRY(fs_label, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_acquire, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_relinquish_self, SC_NORETURN),
    SYSCALL_ENTRY(vcpu_suspend, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_resume, SC_ERRNO),
    SYSCALL_ENTRY(sigsend, SC_ERRNO),
    SYSCALL_ENTRY(sigurgent, SC_VOID),
    SYSCALL_ENTRY(excpt_sethandler, SC_ERRNO),
    SYSCALL_ENTRY(proc_exec, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_policy, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_setpolicy, SC_ERRNO),
    SYSCALL_ENTRY(clock_getres, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_info, SC_ERRNO),
    SYSCALL_ENTRY(nullsys, SC_ERRNO),
    SYSCALL_ENTRY(proc_schedparam, SC_ERRNO),
    SYSCALL_ENTRY(proc_setschedparam, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_state, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_setstate, SC_ERRNO),
    SYSCALL_ENTRY(fs_setlabel, SC_ERRNO),
};

////////////////////////////////////////////////////////////////////////////////

static void _handle_pending_signals(vcpu_t _Nonnull vp)
{
    const sigset_t sigs = vp->pending_sigs;

    if ((sigs & _SIGBIT(SIGKILL)) != 0) {
        Process_Exit(vp->proc, JREASON_SIGNAL, SIGKILL);
        /* NOT REACHED */
    }

    if ((sigs & _SIGBIT(SIGVPRQ)) != 0) {
        Process_RelinquishVirtualProcessor(vp->proc, vp);
        /* NOT REACHED */
    }

    if ((sigs & _SIGBIT(SIGVPDS)) != 0) {
        vcpu_do_pending_deferred_suspend(vp);
    }
}

void _syscall_handler(vcpu_t _Nonnull vp, const syscall_args_t* _Nonnull args)
{
    const unsigned int scno = args->scno;
    intptr_t r;
    char rty;

    if (scno < SYSCALL_COUNT) {
        const syscall_entry_t* sc = &g_syscall_table[scno];

        r = sc->f(vp, args);
        rty = sc->ret_type;
    }
    else {
        r = ENOSYS;
        rty = SC_ERRNO;
    }


    while ((vp->pending_sigs & SIGSET_URGENTS) != 0) {
        _handle_pending_signals(vp);
    }


    switch (rty) {
        case SC_INT:
            syscall_setresult_int(vp, r);
            break;

        case SC_ERRNO:
            if (r == 0) {
                syscall_setresult_int(vp, 0);
            }
            else {
                vp->uerrno = (errno_t)r;
                syscall_setresult_int(vp, -1);
            }
            break;

        case SC_PTR:
            syscall_setresult_ptr(vp, r);
            break;

        case SC_VOID:
            // no return value
            break;
    }
}
