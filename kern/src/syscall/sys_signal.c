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

SYSCALL_4(sig_route, int op, int signo, int target, id_t id)
{
    return Process_Sigroute(vp->proc, pa->op, pa->signo, pa->target, pa->id);
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

SYSCALL_3(sig_send, int target, id_t id, int signo)
{
    ProcessRef pp = vp->proc;

    if (pa->target == SIG_TARGET_VCPU || pa->target == SIG_TARGET_VCPU_GROUP) {
        return Process_ReceiveInternalSignal(pp, pa->target, pa->id, pa->signo);
    }

    if (pa->target == SIG_TARGET_PROC && (pa->id == PROC_SELF || pa->id == pp->pid)) {
        // Sending a signal to ourselves
        return Process_ReceiveInternalSignal(pp, SIG_TARGET_PROC, pa->id, pa->signo);
    }


    // Sending a signal to some other process
    id_t target_id;

    switch (pa->target) {
        case SIG_TARGET_PROC:
            target_id = pa->id;     // proc_self case is handled above 
            break;

        case SIG_TARGET_PROC_CHILDREN:
            target_id = (pa->id == PROC_SELF) ? pp->pid : pa->id;
            break;

        case SIG_TARGET_PROC_GROUP:
            target_id = (pa->id == PROC_SELF) ? pp->pgrp : pa->id;
            break;

        case SIG_TARGET_SESSION:
            target_id = (pa->id == PROC_SELF) ? pp->sid : pa->id;
            break;

        default:
            return EINVAL;
    }

    return Process_SendSignal(pp, pa->target, target_id, 0, pa->signo);
}
