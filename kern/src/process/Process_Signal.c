//
//  Process_Signal.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include "kerneld.h"
#include <assert.h>
#include <kern/kalloc.h>
#include <kern/sigset.h>

errno_t Process_SendSignal(ProcessRef _Nonnull self, int target, pid_t id, vcpuid_t vid, int signo)
{
    sig_sndr_t sndr;
    sig_rcvr_t rcvr;

    sndr.pid = self->pid;
    sndr.uid = FileManager_GetRealUserId(&self->fm);

    rcvr.target = target;
    rcvr.id = id;
    rcvr.vid = vid;

    return ProcessManager_SendSignal(gProcessManager, &sndr, &rcvr, signo);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Signal Routing

static errno_t sigroute_create(int signo, int target, id_t id, sigroute_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    sigroute_t self = NULL;

    err = kalloc(sizeof(struct sigroute), (void**)&self);
    if (err == EOK) {
        self->qe = QUEUE_NODE_INIT;
        self->signo = signo;
        self->target = target;
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

static sigroute_t _Nullable _find_specific_sigroute(ProcessRef _Nonnull _Locked self, int signo, int target, id_t id, sigroute_t* _Nullable pOutPrevEntry)
{
    sigroute_t prp = NULL;

    queue_for_each(&self->sig_route[signo - 1], struct sigroute, it,
        if (it->target == target && it->target_id == id) {
            if (pOutPrevEntry) {
                *pOutPrevEntry = prp;
            }
            return it;
        }

        prp = it;
    )

    return NULL;
}


static errno_t _add_sigroute(ProcessRef _Nonnull _Locked self, int signo, int target, id_t id)
{
    decl_try_err();
    sigroute_t prp;
    sigroute_t rp = _find_specific_sigroute(self, signo, target, id, &prp);

    if (rp == NULL) {
        try(sigroute_create(signo, target, id, &rp));
        queue_add_last(&self->sig_route[signo - 1], &rp->qe);
    }

    if (rp->use_count == INT16_MAX) {
        throw(EOVERFLOW);
    }
    rp->use_count++;

catch:
    return err;
}

static void _del_sigroute(ProcessRef _Nonnull _Locked self, int signo, int target, id_t id)
{
    sigroute_t prp;
    sigroute_t rp = _find_specific_sigroute(self, signo, target, id, &prp);

    if (rp) {
        rp->use_count--;
        if (rp->use_count <= 0) {
            queue_remove(&self->sig_route[signo - 1], &prp->qe, &rp->qe);
            sigroute_destroy(rp);
        }
    }
}

errno_t Process_Sigroute(ProcessRef _Nonnull self, int op, int signo, int target, id_t id)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX || (target != SIG_TARGET_VCPU && target != SIG_TARGET_VCPU_GROUP)) {
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
            err = _add_sigroute(self, signo, target, id);
            break;

        case SIG_ROUTE_DEL:
            _del_sigroute(self, signo, target, id);
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
    if (self->terminator_vcpu) {
        // termination is already in progress
        return;
    }

    self->signo_causing_termination = signo;
    self->terminator_vcpu = vcpu_from_owner_qe(self->vcpu_queue.first);

    vcpu_send_signal(self->terminator_vcpu, SIG_TERMINATE);
    vcpu_resume(self->terminator_vcpu, true);
}

// This is how signals targeting a process are handled:
//
// SIG_TERMINATE:   send to first vcpu in process. This vcpu becomes the termination coordinator
// SIG_FORCED_STOP: broadcast to all vcpus in the process
//
// All other signals may be routed to a vcpu that the process designated. If a
// signal doesn't get routed then default handling kicks in:
//
// SIG_CPU_LIMIT:   terminate process
// SIG_LOGOUT:      terminate process
// SIG_QUIT:        terminate process
// SIG_BKG_READ:    stop process
// SIG_BKG_WRITE:   stop process
// SIG_STOP:        stop process
static void _proc_send_signal_to_proc(ProcessRef _Nonnull _Locked self, int signo)
{
    switch (signo) {
        case SIG_TERMINATE:
            _proc_trigger_termination(self, signo);
            break;

        case SIG_FORCE_STOP:
            _proc_stop(self, WAIT_REASON_SIGNALED, signo, true);
            break;

        case SIG_CONTINUE:
            _proc_continue(self, WAIT_REASON_SIGNALED, signo, true);
            break;

        default:
            if (!queue_empty(&self->sig_route[signo - 1])) {
                queue_for_each(&self->sig_route[signo - 1], struct sigroute, it,
                    switch (it->target) {
                        case SIG_TARGET_VCPU:
                            _proc_send_signal_to_vcpu(self, it->target_id, signo, false);
                            break;

                        case SIG_TARGET_VCPU_GROUP:
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
                    case SIG_STOP:
                        _proc_stop(self, WAIT_REASON_SIGNALED, signo, true);
                        break;

                    default:
                        // Ignore the signal
                        break;
                }
            }
            break;
    }
}

errno_t Process_ReceiveSignal(ProcessRef _Nonnull self, const sig_sndr_t* _Nonnull sndr, int target, vcpuid_t vid, int signo)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    err = perm_check_send_signal(sndr, target, self->pid, FileManager_GetRealUserId(&self->fm), signo);
    if (err != EOK) {
        return err;
    }

    return Process_ReceiveInternalSignal(self, target, vid, signo);
}

errno_t Process_ReceiveInternalSignal(ProcessRef _Nonnull self, int target, vcpuid_t vid, int signo)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    mtx_lock(&self->mtx);
    if ((self->flags & PROC_FLAG_TERMINATING) == 0) {
        switch (target) {
            case SIG_TARGET_VCPU:
                err = _proc_send_signal_to_vcpu(self, vid, signo, true);
                break;

            case SIG_TARGET_VCPU_GROUP:
                err = _proc_send_signal_to_vcpu_group(self, vid, signo);
                break;

            case SIG_TARGET_PROC:
                _proc_send_signal_to_proc(self, signo);
                break;

            default:
                err = EINVAL;
                break;
        }
    }
    else if (signo == SIG_CHILD && self->terminator_vcpu) {
        // Auto-route SIG_CHILD to the exit coordinator because the process is terminating
        vcpu_send_signal(self->terminator_vcpu, signo);
    }
    mtx_unlock(&self->mtx);

    return err;
}
