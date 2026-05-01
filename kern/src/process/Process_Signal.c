//
//  Process_Signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "kerneld.h"
#include <assert.h>
#include <kern/kalloc.h>
#include <kern/signal.h>

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Signal Routing

static errno_t sigroute_create(int signo, int scope, id_t id, sigroute_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    sigroute_t self = NULL;

    err = kalloc(sizeof(struct sigroute), (void**)&self);
    if (err == EOK) {
        self->qe = QUEUE_NODE_INIT;
        self->signo = signo;
        self->scope = scope;
        self->target_id = id;
        self->use_count = 0;
    }

    *pOutSelf = self;
    return err;
}

static void sigroute_destroy(sigroute_t _Nullable self)
{
    kfree(self);
}


void _proc_init_default_sigroutes(ProcessRef _Nonnull _Locked self)
{
    for (size_t i = 0; i < SIG_MAX; i++) {
        self->sig_route[i] = QUEUE_INIT;
    }
}

void _proc_destroy_sigroutes(ProcessRef _Nonnull _Locked self)
{
    for (size_t i = 0; i < SIG_MAX; i++) {
        while (!queue_empty(&self->sig_route[i])) {
            sigroute_t rp = (sigroute_t)queue_remove_first(&self->sig_route[i]);
            sigroute_destroy(rp);
        }
    }
}

static sigroute_t _Nullable _find_specific_sigroute(ProcessRef _Nonnull _Locked self, int signo, int scope, id_t id, sigroute_t* _Nullable pOutPrevEntry)
{
    sigroute_t prp = NULL;

    queue_for_each(&self->sig_route[signo - 1], struct sigroute, it,
        if (it->scope == scope && it->target_id == id) {
            if (pOutPrevEntry) {
                *pOutPrevEntry = prp;
            }
            return it;
        }

        prp = it;
    )

    return NULL;
}


static errno_t _add_sigroute(ProcessRef _Nonnull _Locked self, int signo, int scope, id_t id)
{
    decl_try_err();
    sigroute_t prp;
    sigroute_t rp = _find_specific_sigroute(self, signo, scope, id, &prp);

    if (rp == NULL) {
        try(sigroute_create(signo, scope, id, &rp));
        queue_add_last(&self->sig_route[signo - 1], &rp->qe);
    }

    if (rp->use_count == INT16_MAX) {
        throw(EOVERFLOW);
    }
    rp->use_count++;

catch:
    return err;
}

static void _del_sigroute(ProcessRef _Nonnull _Locked self, int signo, int scope, id_t id)
{
    sigroute_t prp;
    sigroute_t rp = _find_specific_sigroute(self, signo, scope, id, &prp);

    if (rp) {
        rp->use_count--;
        if (rp->use_count <= 0) {
            queue_remove(&self->sig_route[signo - 1], &prp->qe, &rp->qe);
            sigroute_destroy(rp);
        }
    }
}

errno_t Process_Sigroute(ProcessRef _Nonnull self, int op, int signo, int scope, id_t id)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX || (scope != SIG_SCOPE_VCPU && scope != SIG_SCOPE_VCPU_GROUP)) {
        return EINVAL;
    }
    if ((SIGSET_NON_ROUTABLE & sig_bit(signo)) != 0) {
        return EPERM;
    }
    if (self == gKernelProcess) {
        return EPERM;
    }


    mtx_lock(&self->mtx);
    switch (op) {
        case SIG_ROUTE_ADD:
            if (self->run_state < PROC_STATE_TERMINATING) {
                err = _add_sigroute(self, signo, scope, id);
            }
            else {
                // Don't add new routes if we're exiting
                // XXX Should probably return a different error code here
                err = EOK;
            }
            break;

        case SIG_ROUTE_DEL:
            _del_sigroute(self, signo, scope, id);
            break;

        default:
            err = EINVAL;
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Signal Reception

static errno_t _proc_send_signal_to_vcpu(ProcessRef _Nonnull _Locked self, id_t id, int signo, bool doSelfOpt)
{
    vcpu_t me_vp = vcpu_current();
    vcpu_t target_vp = NULL;

    if (doSelfOpt && (id == VCPUID_SELF || me_vp->id == id)) {
        target_vp = me_vp;
    }
    else {
        deque_for_each(&self->vcpu_queue, deque_node_t, it,
            vcpu_t cvp = vcpu_from_owner_qe(it);
            
            if (cvp->id == id) {
                target_vp = cvp;
                break;
            }
        )
    }

    if (target_vp) {
        // This sig_send() will auto-force-resume the receiving vcpu if we're
        // sending SIG_TERMINATE
        vcpu_send_signal(target_vp, signo);
        return EOK;
    }
    else {
        return ESRCH;
    }
}

static errno_t _proc_send_signal_to_vcpu_group(ProcessRef _Nonnull _Locked self, id_t id, int signo)
{
    bool hasMatch = false;

    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        if (cvp->group_id == id) {
            // This sig_send() will auto-force-resume the receiving vcpu if we're
            // sending SIG_TERMINATE
            vcpu_send_signal(cvp, signo);
            hasMatch = true;
        }
    )

    return (hasMatch) ? EOK : ESRCH;
}

static void _proc_trigger_termination(ProcessRef _Nonnull _Locked self, int signo)
{
    self->signo_causing_termination = signo;
    vcpu_send_signal(vcpu_from_owner_qe(self->vcpu_queue.first), SIG_TERMINATE);
}

static errno_t _proc_send_signal_to_proc(ProcessRef _Nonnull _Locked self, id_t id, int signo)
{
    switch (signo) {
        case SIG_TERMINATE:
            _proc_trigger_termination(self, signo);
            break;

        case SIG_FORCE_SUSPEND:
            _proc_suspend(self, WAIT_REASON_SIGNALED, signo, true);
            break;

        case SIG_RESUME:
            _proc_resume(self, WAIT_REASON_SIGNALED, signo, true);
            break;

        default:
            if (!queue_empty(&self->sig_route[signo - 1])) {
                queue_for_each(&self->sig_route[signo - 1], struct sigroute, it,
                    switch (it->scope) {
                        case SIG_SCOPE_VCPU:
                            _proc_send_signal_to_vcpu(self, it->target_id, signo, false);
                            break;

                        case SIG_SCOPE_VCPU_GROUP:
                            _proc_send_signal_to_vcpu_group(self, it->target_id, signo);
                            break;

                        default:
                            abort();
                    }
                )
            }
            else {
                switch (signo) {
                    case SIG_CPU_LIMIT:
                    case SIG_LOGOUT:
                    case SIG_QUIT:
                        _proc_trigger_termination(self, signo);
                        break;

                    case SIG_BKG_READ:
                    case SIG_BKG_WRITE:
                    case SIG_SUSPEND:
                        _proc_suspend(self, WAIT_REASON_SIGNALED, signo, true);
                        break;

                    default:
                        // Ignore the signal
                        break;
                }
            }
            break;
    }

    return EOK;
}

errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }
    if ((SIGSET_PRIV_SYS & sig_bit(signo)) != 0) {
        return EPERM;
    }

    mtx_lock(&self->mtx);
    if (self->run_state < PROC_STATE_TERMINATING) {
        switch (scope) {
            case SIG_SCOPE_VCPU:
                err = _proc_send_signal_to_vcpu(self, id, signo, true);
                break;

            case SIG_SCOPE_VCPU_GROUP:
                err = _proc_send_signal_to_vcpu_group(self, id, signo);
                break;

            case SIG_SCOPE_PROC:
                if (self != gKernelProcess) {
                    err = _proc_send_signal_to_proc(self, id, signo);
                }
                else {
                    err = EPERM;
                }
                break;

            default:
                err = EINVAL;
                break;
        }
    }
    else if (self->run_state == PROC_STATE_TERMINATING && signo == SIG_CHILD && self->exit_coordinator) {
        // Auto-route SIG_CHILD to the exit coordinator because we're in EXIT state
        vcpu_send_signal(self->exit_coordinator, signo);
    }
    mtx_unlock(&self->mtx);

    return err;
}
