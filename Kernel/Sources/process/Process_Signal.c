//
//  Process_Signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"


// Suspend all vcpus in the process if the process is currently in running state.
// Otherwise does nothing. Nesting is not supported.
static void _proc_stop(ProcessRef _Nonnull _Locked self)
{
    if (self->state == PROC_STATE_RUNNING) {
        List_ForEach(&self->vcpu_queue, ListNode,
            vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

            vcpu_suspend(cvp);
        );
        self->state = PROC_STATE_STOPPED;
    }
}

// Resume all vcpus in the process if the process is currently in stopped state.
// Otherwise does nothing.
static void _proc_cont(ProcessRef _Nonnull _Locked self)
{
    if (self->state == PROC_STATE_STOPPED) {
        List_ForEach(&self->vcpu_queue, ListNode,
            vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

            vcpu_resume(cvp, false);
        );
        self->state = PROC_STATE_RUNNING;
    }
}

static errno_t _proc_send_signal_to_vcpu(ProcessRef _Nonnull self, id_t id, int signo)
{
    vcpu_t me_vp = vcpu_current();
    vcpu_t target_vp = NULL;

    if (id == VCPUID_SELF || me_vp->id == id) {
        target_vp = me_vp;
    }
    else {
        List_ForEach(&self->vcpu_queue, ListNode,
            vcpu_t cvp = vcpu_from_owner_qe(pCurNode);
            
            if (cvp->id == id) {
                target_vp = cvp;
                break;
            }
        );
    }

    if (target_vp) {
        // This sigsend() will auto-force-resume the receiving vcpu if we're
        // sending SIGKILL
        vcpu_sigsend(target_vp, signo, SIG_SCOPE_VCPU);
        return EOK;
    }
    else {
        return ESRCH;
    }
}

static errno_t _proc_send_signal_to_vcpu_group(ProcessRef _Nonnull self, id_t id, int signo)
{
    bool hasMatch = false;

    List_ForEach(&self->vcpu_queue, ListNode,
        vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

        if (cvp->groupid == id) {
            // This sigsend() will auto-force-resume the receiving vcpu if we're
            // sending SIGKILL
            vcpu_sigsend(cvp, signo, SIG_SCOPE_VCPU_GROUP);
            hasMatch = true;
        }
    );

    return (hasMatch) ? EOK : ESRCH;
}

static errno_t _proc_send_signal_to_proc(ProcessRef _Nonnull self, id_t id, int signo)
{
    switch (signo) {
        case SIGKILL:
            vcpu_sigsend(vcpu_from_owner_qe(self->vcpu_queue.first), SIGKILL, SIG_SCOPE_PROC);
            break;

        case SIGSTOP:
            _proc_stop(self);
            break;

        case SIGCONT:
            _proc_cont(self);
            break;

        default:
            List_ForEach(&self->vcpu_queue, ListNode,
                vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

                // This sigsend() will auto-force-resume the receiving vcpu if we're
                // sending SIGKILL
                vcpu_sigsend(cvp, signo, SIG_SCOPE_PROC);
            );
            break;
    }

    return EOK;
}

errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo)
{
    decl_try_err();

    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }

    mtx_lock(&self->mtx);
    switch (scope) {
        case SIG_SCOPE_VCPU:
            err = _proc_send_signal_to_vcpu(self, id, signo);
            break;

        case SIG_SCOPE_VCPU_GROUP:
            err = _proc_send_signal_to_vcpu_group(self, id, signo);
            break;

        case SIG_SCOPE_PROC:
            err = _proc_send_signal_to_proc(self, id, signo);
            break;

        default:
            err = EINVAL;
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}
