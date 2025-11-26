//
//  Process_Signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <kern/kalloc.h>

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Signal Routing

static errno_t sigroute_create(int signo, int scope, id_t id, sigroute_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    sigroute_t self = NULL;

    err = kalloc(sizeof(struct sigroute), (void**)&self);
    if (err == EOK) {
        self->qe = SLISTNODE_INIT;
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
    for (size_t i = 0; i < SIGMAX; i++) {
        self->sig_route[i] = SLIST_INIT;
    }
}

void _proc_destroy_sigroutes(ProcessRef _Nonnull _Locked self)
{
    for (size_t i = 0; i < SIGMAX; i++) {
        while (!SList_IsEmpty(&self->sig_route[i])) {
            sigroute_t rp = (sigroute_t)SList_RemoveFirst(&self->sig_route[i]);
            sigroute_destroy(rp);
        }
    }
}

static sigroute_t _Nullable _find_specific_sigroute(ProcessRef _Nonnull _Locked self, int signo, int scope, id_t id, sigroute_t* _Nullable pOutPrevEntry)
{
    sigroute_t prp = NULL;

    SList_ForEach(&self->sig_route[signo - 1], ListNode,
        sigroute_t crp = (sigroute_t)pCurNode;

        if (crp->scope == scope && crp->target_id == id) {
            if (pOutPrevEntry) {
                *pOutPrevEntry = prp;
            }
            return crp;
        }

        prp = crp;
    );

    return NULL;
}


static errno_t _add_sigroute(ProcessRef _Nonnull _Locked self, int signo, int scope, id_t id)
{
    decl_try_err();
    sigroute_t prp;
    sigroute_t rp = _find_specific_sigroute(self, signo, scope, id, &prp);

    if (rp == NULL) {
        try(sigroute_create(signo, scope, id, &rp));
        SList_InsertAfterLast(&self->sig_route[signo - 1], &rp->qe);
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
            SList_Remove(&self->sig_route[signo - 1], &prp->qe, &rp->qe);
            sigroute_destroy(rp);
        }
    }
}

errno_t Process_Sigroute(ProcessRef _Nonnull self, int op, int signo, int scope, id_t id)
{
    decl_try_err();

    if (signo < SIGMIN || signo > SIGMAX || (scope != SIG_SCOPE_VCPU && scope != SIG_SCOPE_VCPU_GROUP)) {
        return EINVAL;
    }
    if (signo == SIGKILL || signo == SIGSTOP || signo == SIGCONT || signo == SIGVRLQ || signo == SIGVSPD) {
        return EPERM;
    }


    mtx_lock(&self->mtx);
    switch (op) {
        case SIG_ROUTE_ADD:
            if (self->state < PROC_STATE_EXITING) {
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

static void _proc_terminate_on_behalf_of(ProcessRef _Nonnull _Locked self, int signo)
{
    _proc_set_exit_reason(self, JREASON_SIGNAL, signo)
    vcpu_sigsend(vcpu_from_owner_qe(self->vcpu_queue.first), SIGKILL);
}

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

static errno_t _proc_send_signal_to_vcpu(ProcessRef _Nonnull _Locked self, id_t id, int signo, bool doSelfOpt)
{
    vcpu_t me_vp = vcpu_current();
    vcpu_t target_vp = NULL;

    if (doSelfOpt && (id == VCPUID_SELF || me_vp->id == id)) {
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
        vcpu_sigsend(target_vp, signo);
        return EOK;
    }
    else {
        return ESRCH;
    }
}

static errno_t _proc_send_signal_to_vcpu_group(ProcessRef _Nonnull _Locked self, id_t id, int signo)
{
    bool hasMatch = false;

    List_ForEach(&self->vcpu_queue, ListNode,
        vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

        if (cvp->groupid == id) {
            // This sigsend() will auto-force-resume the receiving vcpu if we're
            // sending SIGKILL
            vcpu_sigsend(cvp, signo);
            hasMatch = true;
        }
    );

    return (hasMatch) ? EOK : ESRCH;
}

static errno_t _proc_send_signal_to_proc(ProcessRef _Nonnull _Locked self, id_t id, int signo)
{
    switch (signo) {
        case SIGKILL:
            _proc_terminate_on_behalf_of(self, signo);
            break;

        case SIGSTOP:
            _proc_stop(self);
            break;

        case SIGCONT:
            _proc_cont(self);
            break;

        default:
            if (!SList_IsEmpty(&self->sig_route[signo - 1])) {
                SList_ForEach(&self->sig_route[signo - 1], SList, {
                    sigroute_t crp = (sigroute_t)pCurNode;

                    switch (crp->scope) {
                        case SIG_SCOPE_VCPU:
                            _proc_send_signal_to_vcpu(self, crp->target_id, signo, false);
                            break;

                        case SIG_SCOPE_VCPU_GROUP:
                            _proc_send_signal_to_vcpu_group(self, crp->target_id, signo);
                            break;

                        default:
                            abort();
                    }
                })
            }
            else {
                switch (signo) {
                    case SIGABRT:
                    case SIGXCPU:
                    case SIGHUP:
                    case SIGQUIT:
                        _proc_terminate_on_behalf_of(self, signo);
                        break;

                    case SIGTTIN:
                    case SIGTTOUT:
                    case SIGTSTP:
                        _proc_stop(self);
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

    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }
    if (signo == SIGVRLQ || signo == SIGVSPD) {
        return EPERM;
    }

    mtx_lock(&self->mtx);
    if (self->state < PROC_STATE_EXITING) {
        switch (scope) {
            case SIG_SCOPE_VCPU:
                err = _proc_send_signal_to_vcpu(self, id, signo, true);
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
    }
    else if (self->state == PROC_STATE_EXITING && signo == SIGCHILD && self->exit_coordinator) {
        // Auto-route SIGCHILD to the exit coordinator because we're in EXIT state
        vcpu_sigsend(self->exit_coordinator, signo);
    }
    mtx_unlock(&self->mtx);

    return err;
}
