//
//  sys_signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kern/timespec.h>
#include <kpi/signal.h>
#include <machine/csw.h>
#include <process/ProcessManager.h>
#include <sched/waitqueue.h>


SYSCALL_0(sigurgent)
{
    return EOK;
}

//XXX add support for SIG_SCOPE_VCPU_GROUP
SYSCALL_3(sigroute, int scope, id_t id, int op)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    vcpu_t the_vp = NULL;

    if (pa->scope != SIG_SCOPE_VCPU) {
        return EINVAL;
    }

    mtx_lock(&pp->mtx);
    if (pa->id != 0) {
        List_ForEach(&pp->vpQueue, ListNode, 
            vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);

            if (cvp->id == pa->id) {
                the_vp = cvp;
                break;
            }
        );
    }
    else {
        the_vp = vp;
    }

    if (the_vp) {
        vcpu_sigroute(the_vp, pa->op);
    }
    else {
        err = ESRCH;
    }
    mtx_unlock(&pp->mtx);
}

SYSCALL_2(sigwait, const sigset_t* _Nonnull set, int* _Nullable signo)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    err = vcpu_sigwait(&pp->siwaQueue, pa->set, pa->signo);
    preempt_restore(sps);
    return err;
}

SYSCALL_4(sigtimedwait, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nullable signo)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    const int sps = preempt_disable();
    err = vcpu_sigtimedwait(&pp->siwaQueue, pa->set, pa->flags, pa->wtp, pa->signo);
    preempt_restore(sps);
    return err;
}

SYSCALL_1(sigpending, sigset_t* _Nonnull set)
{
    decl_try_err();

    const int sps = preempt_disable();
    *(pa->set) = vp->pending_sigs;
    preempt_restore(sps);
    
    return EOK;
}

SYSCALL_3(sigsend, int scope, id_t id, int signo)
{
    decl_try_err();
    ProcessRef pp = vp->proc;

    switch (pa->scope) {
        case SIG_SCOPE_VCPU:
        case SIG_SCOPE_VCPU_GROUP:
            err = Process_SendSignal(pp, pa->scope, pa->id, pa->signo);
            break;

        case SIG_SCOPE_PROC:
            if (pa->id == 0) {
                err = Process_SendSignal(pp, SIG_SCOPE_PROC, pa->id, pa->signo);
            }
            else {
                err = ProcessManager_SendSignal(gProcessManager, pp->sid, SIG_SCOPE_PROC, pa->id, pa->signo);
            }
            break;

        case SIG_SCOPE_PROC_CHILDREN: {
            const pid_t pid = (pa->id == 0) ? pp->pid : pa->id;
            err = ProcessManager_SendSignal(gProcessManager, pp->sid, pa->scope, pid, pa->signo);
            break;
        }

        case SIG_SCOPE_PROC_GROUP: {
            const pid_t pgrp = (pa->id == 0) ? pp->pgrp : pa->id;
            err = ProcessManager_SendSignal(gProcessManager, pp->sid, pa->scope, pgrp, pa->signo);
            break;
        }

        case SIG_SCOPE_SESSION: {
            const pid_t sid = (pa->id == 0) ? pp->sid : pa->id;

            if (sid == pp->sid) {
                err = ProcessManager_SendSignal(gProcessManager, pp->sid, pa->scope, sid, pa->signo);
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
