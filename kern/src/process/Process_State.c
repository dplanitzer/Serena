//
//  Process_State.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"


int Process_GetState(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int state = self->run_state;
    mtx_unlock(&self->mtx);

    return state;
}

void _proc_setstate(ProcessRef _Nonnull _Locked self, int state, bool signal)
{
    self->run_state = state;

    if (signal) {
        sigcred_t sc;

        Process_GetSigcred(self, &sc);
        ProcessManager_SendSignal(gProcessManager, &sc, SIG_SCOPE_PROC, self->ppid, SIG_CHILD);
    }
}

static ProcessRef _Nullable _find_matching_zombie(ProcessRef _Nonnull self, int match, pid_t id, bool* _Nonnull pOutExists)
{
    switch (match) {
        case WAIT_PID:
            return ProcessManager_CopyZombieOfParent(gProcessManager, self->pid, id, pOutExists);

        case WAIT_GROUP:
            return ProcessManager_CopyGroupZombieOfParent(gProcessManager, self->pid, id, pOutExists);

        case WAIT_ANY:
            return ProcessManager_CopyAnyZombieOfParent(gProcessManager, self->pid, pOutExists);

        default:
            return NULL;
    }
}

errno_t Process_WaitForState(ProcessRef _Nonnull self, int wstate, int match, pid_t id, int flags, proc_waitres_t* _Nonnull res)
{
    decl_try_err();
    ProcessRef zp = NULL;

    switch (wstate) {
        case WAIT_FOR_ANY:
        case WAIT_FOR_RESUMED:
        case WAIT_FOR_SUSPENDED:
        case WAIT_FOR_TERMINATED:
            break;

        default:
            return EINVAL;
    }

    switch (match) {
        case WAIT_PID:
        case WAIT_GROUP:
        case WAIT_ANY:
            break;

        default:
            return EINVAL;
    }

    if ((flags & ~(WAIT_NONBLOCKING)) != 0) {
        return EINVAL;
    }


    for (;;) {
        bool exists = false;

        zp = _find_matching_zombie(self, match, id, &exists);

        if (zp) {
            break;
        }

        if (!exists) {
            return ECHILD;
        }
        
        if ((flags & WAIT_NONBLOCKING) == WAIT_NONBLOCKING) {
            return EAGAIN;
        }


        sigset_t hot_sigs = sig_bit(SIG_CHILD);
        int signo;

        err = vcpu_wait_for_signal(&self->siwa_queue, &hot_sigs, &signo);
        if (err != EOK) {
            return err;
        }
    }


    res->pid = zp->pid;
    res->state = PROC_STATE_TERMINATED;
    res->reason = zp->exit_reason;
    res->u.status = zp->exit_code;

    ProcessManager_Unpublish(gProcessManager, zp);
    Process_Release(zp); // necessary because of the _find_matching_zombie() above

    return EOK;
}
