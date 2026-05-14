//
//  Process_State.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"

errno_t Process_GetMatchingState(ProcessRef _Nonnull self, int mstate, proc_waitres_t* _Nonnull res)
{
    decl_try_err();
    bool hasMatch = false;

    mtx_lock(&self->mtx);
    if (self->run_state_reason.reason != _WAIT_REASON_NONE) {
        // We only match (and care) if a reason for the most recent state change
        // has been provided.

        switch (mstate) {
            case WAIT_FOR_ANY:
                hasMatch = true;
                break;

            case WAIT_FOR_RESUMED:
                hasMatch = (self->run_state == PROC_STATE_RUNNING);
                break;

            case WAIT_FOR_SUSPENDED:
                hasMatch = (self->run_state == PROC_STATE_STOPPED);
                break;

            case WAIT_FOR_TERMINATED:
                hasMatch = (self->run_state == PROC_STATE_TERMINATED);
                break;

            default:
                err = EINVAL;
                break;
        }
    }

    if (hasMatch) {
        res->pid = self->pid;
        res->state = self->run_state;
        res->reason = self->run_state_reason.reason;

        switch (self->run_state_reason.reason) {
            case WAIT_REASON_EXITED:
                res->u.status = self->run_state_reason.u.exit_code;
                break;

            case WAIT_REASON_SIGNALED:
                res->u.signo = self->run_state_reason.u.signo;
                break;

            case WAIT_REASON_EXCEPTION:
                res->u.excptno = self->run_state_reason.u.excptno;
                break;
        }

        // Consume the status
        self->run_state_reason.reason = _WAIT_REASON_NONE;
        err = EOK;
    }
    else {
        err = EAGAIN;
    }
    mtx_unlock(&self->mtx);

    return err;
}

void _proc_set_state(ProcessRef _Nonnull _Locked self, int state, int reason, intptr_t arg, bool notify_parent)
{
    self->run_state = state;
    self->run_state_reason.reason = reason;

    switch (reason) {
        case _WAIT_REASON_NONE:
            break;

        case WAIT_REASON_EXITED:
            self->run_state_reason.u.exit_code = (int)arg;
            break;

        case WAIT_REASON_SIGNALED:
            self->run_state_reason.u.signo = (int)arg;
            break;

        case WAIT_REASON_EXCEPTION:
            self->run_state_reason.u.excptno = (int)arg;
            break;

        default:
            abort();
    }

    if (notify_parent) {
        Process_SendSignal(self, SIG_TARGET_PROC, self->ppid, 0, SIG_CHILD);
    }
}

errno_t Process_WaitForState(ProcessRef _Nonnull self, int wstate, int match, pid_t id, int flags, proc_waitres_t* _Nonnull res)
{
    decl_try_err();
    const sigset_t hot_sigs = sig_bit(SIG_CHILD);
    const sigwait_flags = ((flags & _WAIT_NOCANCEL) == _WAIT_NOCANCEL) ? SIGWAIT_NOABORT : 0;
    int signo;

    for (;;) {
        err = ProcessManager_GetStatusForProcessMatchingState(gProcessManager, wstate, self->pid, match, id, res);

        if (err == EOK || err != EAGAIN) {
            break;
        }
        
        if ((flags & WAIT_NONBLOCKING) == WAIT_NONBLOCKING) {
            return EAGAIN;
        }

        err = vcpu_sigwait(&self->siwa_queue, &hot_sigs, sigwait_flags, NULL, &signo);
        if (err == ECANCELED) {
            break;
        }
    }

    return err;
}
