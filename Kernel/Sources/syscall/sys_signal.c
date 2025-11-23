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
#include <machine/sched.h>
#include <process/ProcessManager.h>
#include <sched/waitqueue.h>


SYSCALL_0(sigurgent)
{
    return EOK;
}

SYSCALL_4(sigroute, int op, int signo, int scope, id_t id)
{
    return Process_Sigroute(vp->proc, pa->op, pa->signo, pa->scope, pa->id);
}

SYSCALL_2(sigwait, const sigset_t* _Nonnull set, int* _Nullable signo)
{
    ProcessRef pp = vp->proc;

    return vcpu_sigwait(&pp->siwa_queue, pa->set, pa->signo);
}

SYSCALL_4(sigtimedwait, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nullable signo)
{
    ProcessRef pp = vp->proc;

    return vcpu_sigtimedwait(&pp->siwa_queue, pa->set, pa->flags, pa->wtp, pa->signo);
}

SYSCALL_1(sigpending, sigset_t* _Nonnull set)
{
    *(pa->set) = vcpu_sigpending(vp);
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
