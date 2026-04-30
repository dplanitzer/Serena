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

SYSCALL_REF(clock_time);
SYSCALL_REF(clock_wait);
SYSCALL_REF(clock_info);

SYSCALL_REF(excpt_sethandler);

SYSCALL_REF(host_info);
SYSCALL_REF(host_processes);
SYSCALL_REF(host_filesystems);

SYSCALL_REF(fs_info);
SYSCALL_REF(fs_property);
SYSCALL_REF(fs_setlabel);
SYSCALL_REF(fs_mount);
SYSCALL_REF(fs_unmount);
SYSCALL_REF(fs_sync);
SYSCALL_REF(fs_create_file);
SYSCALL_REF(fs_open);
SYSCALL_REF(fs_open_directory);
SYSCALL_REF(fs_create_directory);
SYSCALL_REF(fs_attr);
SYSCALL_REF(fs_truncate);
SYSCALL_REF(fs_access);
SYSCALL_REF(fs_unlink);
SYSCALL_REF(fs_rename);
SYSCALL_REF(fs_setowner);
SYSCALL_REF(fs_setperms);
SYSCALL_REF(fs_settimes);

SYSCALL_REF(fd_close);
SYSCALL_REF(fd_read);
SYSCALL_REF(fd_write);
SYSCALL_REF(fd_seek);
SYSCALL_REF(fd_truncate);
SYSCALL_REF(fd_attr);
SYSCALL_REF(fd_info);
SYSCALL_REF(fd_setflags);
SYSCALL_REF(fd_dup);
SYSCALL_REF(fd_dup_to);
SYSCALL_REF(ioctl);

SYSCALL_REF(pipe_create);

SYSCALL_REF(proc_exit);
SYSCALL_REF(proc_spawn);
SYSCALL_REF(proc_property);
SYSCALL_REF(proc_setcwd);
SYSCALL_REF(proc_status);
SYSCALL_REF(proc_exec);
SYSCALL_REF(proc_schedparam);
SYSCALL_REF(proc_setschedparam);
SYSCALL_REF(proc_info);
SYSCALL_REF(proc_vcpus);
SYSCALL_REF(proc_setumask);
SYSCALL_REF(proc_terminate);
SYSCALL_REF(proc_suspend);
SYSCALL_REF(proc_resume);
SYSCALL_REF(proc_self);


SYSCALL_REF(vm_allocate);

SYSCALL_REF(sig_wait);
SYSCALL_REF(sig_timedwait);
SYSCALL_REF(sig_pending);
SYSCALL_REF(sig_send);
SYSCALL_REF(sig_route);
SYSCALL_REF(sig_urgent);

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

#define SYSCALL_COUNT   80

static const syscall_entry_t g_syscall_table[SYSCALL_COUNT] = {
    SYSCALL_ENTRY(fd_read, SC_ERRNO),
    SYSCALL_ENTRY(fd_write, SC_ERRNO),
    SYSCALL_ENTRY(clock_wait, SC_ERRNO),
    SYSCALL_ENTRY(vm_allocate, SC_ERRNO),
    SYSCALL_ENTRY(proc_exit, SC_NORETURN),
    SYSCALL_ENTRY(proc_spawn, SC_ERRNO),
    SYSCALL_ENTRY(proc_terminate, SC_ERRNO),
    SYSCALL_ENTRY(proc_vcpus, SC_ERRNO),
    SYSCALL_ENTRY(proc_suspend, SC_ERRNO),
    SYSCALL_ENTRY(fs_open, SC_ERRNO),
    SYSCALL_ENTRY(fd_close, SC_ERRNO),
    SYSCALL_ENTRY(proc_status, SC_ERRNO),
    SYSCALL_ENTRY(fd_seek, SC_ERRNO),
    SYSCALL_ENTRY(proc_property, SC_ERRNO),
    SYSCALL_ENTRY(proc_setcwd, SC_ERRNO),
    SYSCALL_ENTRY(host_processes, SC_ERRNO),
    SYSCALL_ENTRY(proc_setumask, SC_VOID),
    SYSCALL_ENTRY(fs_create_directory, SC_ERRNO),
    SYSCALL_ENTRY(fs_attr, SC_ERRNO),
    SYSCALL_ENTRY(fs_open_directory, SC_ERRNO),
    SYSCALL_ENTRY(fs_access, SC_ERRNO),
    SYSCALL_ENTRY(fd_attr, SC_ERRNO),
    SYSCALL_ENTRY(fs_unlink, SC_ERRNO),
    SYSCALL_ENTRY(fs_rename, SC_ERRNO),
    SYSCALL_ENTRY(ioctl, SC_ERRNO),
    SYSCALL_ENTRY(fs_truncate, SC_ERRNO),
    SYSCALL_ENTRY(fd_truncate, SC_ERRNO),
    SYSCALL_ENTRY(fs_create_file, SC_ERRNO),
    SYSCALL_ENTRY(pipe_create, SC_ERRNO),
    SYSCALL_ENTRY(wq_dispose, SC_ERRNO),
    SYSCALL_ENTRY(clock_time, SC_ERRNO),
    SYSCALL_ENTRY(fs_mount, SC_ERRNO),
    SYSCALL_ENTRY(fs_unmount, SC_ERRNO),
    SYSCALL_ENTRY(host_info, SC_ERRNO),
    SYSCALL_ENTRY(fs_sync, SC_ERRNO),
    SYSCALL_ENTRY(coninit, SC_ERRNO),
    SYSCALL_ENTRY(fs_property, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_errno, SC_INT),
    SYSCALL_ENTRY(fs_setowner, SC_ERRNO),
    SYSCALL_ENTRY(fd_setflags, SC_ERRNO),
    SYSCALL_ENTRY(fs_setperms, SC_ERRNO),
    SYSCALL_ENTRY(fs_settimes, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_yield, SC_VOID),
    SYSCALL_ENTRY(wq_create, SC_ERRNO),
    SYSCALL_ENTRY(wq_wait, SC_ERRNO),
    SYSCALL_ENTRY(wq_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_wakeup, SC_ERRNO),
    SYSCALL_ENTRY(proc_info, SC_ERRNO),
    SYSCALL_ENTRY(sig_route, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_getdata, SC_PTR),
    SYSCALL_ENTRY(vcpu_setdata, SC_INT),
    SYSCALL_ENTRY(sig_wait, SC_ERRNO),
    SYSCALL_ENTRY(sig_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(wq_wakeup_then_timedwait, SC_ERRNO),
    SYSCALL_ENTRY(sig_pending, SC_ERRNO),
    SYSCALL_ENTRY(host_filesystems, SC_ERRNO),
    SYSCALL_ENTRY(fs_info, SC_ERRNO),
    SYSCALL_ENTRY(fd_dup_to, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_acquire, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_relinquish_self, SC_NORETURN),
    SYSCALL_ENTRY(vcpu_suspend, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_resume, SC_ERRNO),
    SYSCALL_ENTRY(sig_send, SC_ERRNO),
    SYSCALL_ENTRY(sig_urgent, SC_VOID),
    SYSCALL_ENTRY(excpt_sethandler, SC_ERRNO),
    SYSCALL_ENTRY(proc_exec, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_policy, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_setpolicy, SC_ERRNO),
    SYSCALL_ENTRY(clock_info, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_info, SC_ERRNO),
    SYSCALL_ENTRY(nullsys, SC_ERRNO),
    SYSCALL_ENTRY(proc_schedparam, SC_ERRNO),
    SYSCALL_ENTRY(proc_setschedparam, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_state, SC_ERRNO),
    SYSCALL_ENTRY(vcpu_setstate, SC_ERRNO),
    SYSCALL_ENTRY(fs_setlabel, SC_ERRNO),
    SYSCALL_ENTRY(proc_resume, SC_ERRNO),
    SYSCALL_ENTRY(fd_info, SC_ERRNO),
    SYSCALL_ENTRY(fd_dup, SC_ERRNO),
    SYSCALL_ENTRY(proc_self, SC_INT),
};

////////////////////////////////////////////////////////////////////////////////

static void _handle_pending_signals(vcpu_t _Nonnull vp)
{
    const sigset_t sigs = vp->pending_sigs;

    if ((sigs & sig_bit(SIG_TERMINATE)) != 0) {
        Process_Exit(vp->proc, STATUS_REASON_SIGNALED, SIG_TERMINATE);
        /* NOT REACHED */
    }

    if ((sigs & sig_bit(SIG_VCPU_RELINQUISH)) != 0) {
        Process_RelinquishVirtualProcessor(vp->proc, vp);
        /* NOT REACHED */
    }

    if ((sigs & sig_bit(SIG_VCPU_SUSPEND)) != 0) {
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
