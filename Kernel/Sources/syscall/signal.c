//
//  signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kern/timespec.h>
#include <kpi/signal.h>
#include <sched/waitqueue.h>


SYSCALL_2(sigwait, const sigset_t* _Nonnull set, siginfo_t* _Nullable info)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    err = wq_sigwait(&pp->siwaQueue, pa->set, pa->info);
    preempt_restore(sps);
    return err;
}

SYSCALL_4(sigtimedwait, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, siginfo_t* _Nullable info)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    err = wq_sigtimedwait(&pp->siwaQueue, pa->set, pa->flags, pa->wtp, pa->info);
    preempt_restore(sps);
    return err;
}

SYSCALL_1(sigpending, sigset_t* _Nonnull set)
{
    decl_try_err();
    const int sps = preempt_disable();

    *(pa->set) = vp->psigs & ~vp->sigmask;
    preempt_restore(sps);
    return EOK;
}

static errno_t _sendsig(VirtualProcessor* _Nonnull vp, int signo)
{
    const int sps = preempt_disable();
    const errno_t err = VirtualProcessor_Signal(vp, signo);
    preempt_restore(sps);

    return err;
}

SYSCALL_3(sigsend, int scope, id_t id, int signo)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    bool foundIt = false;

    mtx_lock(&pp->mtx);
    List_ForEach(&pp->vpQueue, ListNode, {
        VirtualProcessor* cvp = VP_FROM_OWNER_NODE(pCurNode);

        if (pa->scope == SIG_SCOPE_VCPU && pa->id == cvp->vpid) {
            _sendsig(cvp, pa->signo);
            foundIt = true;
            break;
        }
        else if (pa->scope == SIG_SCOPE_VCPU_GROUP && pa->id == cvp->vpgid) {
            _sendsig(cvp, pa->signo);
            foundIt = true;
        }
    });
    mtx_unlock(&pp->mtx);

    if (!foundIt) {
        err = ESRCH;
    }

    return err;
}
