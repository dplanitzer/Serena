//
//  sys_signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <assert.h>
#include <ext/nanotime.h>
#include <hal/sched.h>
#include <kpi/signal.h>
#include <process/ProcessManager.h>
#include <sched/waitqueue.h>


SYSCALL_0(sig_urgent)
{
    return EOK;
}

SYSCALL_4(sig_route, int op, int signo, int scope, id_t id)
{
    return Process_Sigroute(vp->proc, pa->op, pa->signo, pa->scope, pa->id);
}

SYSCALL_2(sig_wait, const sigset_t* _Nonnull set, int* _Nullable signo)
{
    ProcessRef pp = vp->proc;

    return vcpu_wait_for_signal(&pp->siwa_queue, pa->set, pa->signo);
}

SYSCALL_4(sig_timedwait, const sigset_t* _Nonnull set, int flags, const nanotime_t* _Nonnull wtp, int* _Nullable signo)
{
    ProcessRef pp = vp->proc;

    return vcpu_timedwait_for_signal(&pp->siwa_queue, pa->set, pa->flags, pa->wtp, pa->signo);
}

SYSCALL_1(sig_pending, sigset_t* _Nonnull set)
{
    *(pa->set) = vcpu_pending_signals(vp);
    return EOK;
}

SYSCALL_3(sig_send, int scope, id_t id, int signo)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    if (pa->scope == SIG_SCOPE_VCPU || pa->scope == SIG_SCOPE_VCPU_GROUP) {
        return Process_SendSignal(pp, pa->scope, pa->id, pa->signo);
    }

    if (pa->scope == SIG_SCOPE_PROC && pa->id == 0) {
        // Send signal to yourself
        return Process_SendSignal(pp, SIG_SCOPE_PROC, pa->id, pa->signo);
    }


    // Sending a signal to some other process
    sigcred_t sndr;
    Process_GetSigcred(pp, &sndr);

    switch (pa->scope) {
        case SIG_SCOPE_PROC:
            err = ProcessManager_SendSignal(gProcessManager, &sndr, SIG_SCOPE_PROC, pa->id, pa->signo);
            break;

        case SIG_SCOPE_PROC_CHILDREN: {
            const pid_t pid = (pa->id == 0) ? pp->pid : pa->id;
            err = ProcessManager_SendSignal(gProcessManager, &sndr, pa->scope, pid, pa->signo);
            break;
        }

        case SIG_SCOPE_PROC_GROUP: {
            const pid_t pgrp = (pa->id == 0) ? pp->pgrp : pa->id;
            err = ProcessManager_SendSignal(gProcessManager, &sndr, pa->scope, pgrp, pa->signo);
            break;
        }

        case SIG_SCOPE_SESSION: {
            const pid_t sid = (pa->id == 0) ? pp->sid : pa->id;

            if (sid == pp->sid) {
                err = ProcessManager_SendSignal(gProcessManager, &sndr, pa->scope, sid, pa->signo);
            }
            else {
                err = EPERM;
            }
            break;
        }

        default:
            abort();
    }

    return err;
}
