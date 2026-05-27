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
    sig_dispatch_t dp;

    dp.sndr.pid = self->pid;
    dp.sndr.uid = FileManager_GetRealUserId(&self->fm);
    dp.rcvr.target = target;
    dp.rcvr.id = id;
    dp.rcvr.vid = vid;
    dp.signo = signo;

    return ProcessManager_SendSignal(gProcessManager, &dp);
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
        self->qe = DEQUE_NODE_INIT;
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


void _proc_destroy_sigroutes(ProcessRef _Nonnull _Locked self)
{
    while (!deque_empty(&self->sig_routes)) {
        sigroute_t rp = (sigroute_t)deque_remove_first(&self->sig_routes);
        sigroute_destroy(rp);
    }
}

void _proc_destroy_sigroutes_except_for_vcpuid(ProcessRef _Nonnull _Locked self, vcpuid_t vid)
{
    deque_for_each(&self->sig_routes, struct sigroute, it,
        if (it->target == SIG_TARGET_VCPU && it->target_id == vid) {
            continue;
        }

        deque_remove(&self->sig_routes, &it->qe);
        sigroute_destroy(it);
    );
}

void _proc_reassign_sigroutes_to_vcpuid(ProcessRef _Nonnull _Locked self, vcpuid_t oldid, vcpuid_t newid)
{
    deque_for_each(&self->sig_routes, struct sigroute, it,
        if (it->target == SIG_TARGET_VCPU && it->target_id == oldid) {
            it->target_id = newid;
        }
    );
}

static sigroute_t _Nullable _find_specific_sigroute(ProcessRef _Nonnull _Locked self, int signo, int target, id_t id)
{
    deque_for_each(&self->sig_routes, struct sigroute, it,
        if (it->signo == signo && it->target == target && it->target_id == id) {
            return it;
        }
    )

    return NULL;
}


static errno_t _add_sigroute(ProcessRef _Nonnull _Locked self, int signo, int target, id_t id)
{
    decl_try_err();
    sigroute_t rp = _find_specific_sigroute(self, signo, target, id);

    if (rp == NULL) {
        try(sigroute_create(signo, target, id, &rp));
        deque_add_last(&self->sig_routes, &rp->qe);
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
    sigroute_t rp = _find_specific_sigroute(self, signo, target, id);

    if (rp) {
        rp->use_count--;
        if (rp->use_count <= 0) {
            deque_remove(&self->sig_routes, &rp->qe);
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
    if (!_proc_is_user(self)) {
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
        // sending SIG_FORCE_QUIT
        vcpu_send_signal(target_vp, signo);
        return EOK;
    }
    else {
        return ESRCH;
    }
}

static errno_t _proc_broadcast_signal_to_vcpu_group(ProcessRef _Nonnull _Locked self, id_t id, int signo)
{
    bool hasMatch = false;

    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        if (cvp->group_id == id) {
            // This sig_send() will auto-force-resume the receiving vcpu if we're
            // sending SIG_FORCE_QUIT
            vcpu_send_signal(cvp, signo);
            hasMatch = true;
        }
    )

    return (hasMatch) ? EOK : ESRCH;
}

static errno_t _proc_send_signal_to_vcpu_group_asam(ProcessRef _Nonnull _Locked self, id_t id, int signo)
{
    // 1. Pass: send signal to a waiting vcpu
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        if (cvp->group_id == id && cvp->run_state == VCPU_STATE_WAITING) {
            vcpu_send_signal(cvp, signo);
            return EOK;
        }
    )


    // 2.Pass: send the signal to any vcpu, no matter what its state is
    //XXX don't just always dump it at the first group member. Choose one pseudo
    //XXX randomly to spread out the load.
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        if (cvp->group_id == id) {
            vcpu_send_signal(cvp, signo);
            return EOK;
        }
    )

    return ESRCH;
}

// Routes signal 'signo' to all vcpus and vcpu groups that are interested in
// receiving it. Returns true if at least one routes exists for this signal and
// false otherwise.
static bool _proc_route_signal(ProcessRef _Nonnull _Locked self, int signo)
{
    bool hasRoute = false;

    deque_for_each(&self->sig_routes, struct sigroute, it,
        if (it->signo == signo) {
            hasRoute = true;

            switch (it->target) {
                case SIG_TARGET_VCPU:
                    _proc_send_signal_to_vcpu(self, it->target_id, signo, false);
                    break;

                case SIG_TARGET_VCPU_GROUP:
                    _proc_broadcast_signal_to_vcpu_group(self, it->target_id, signo);
                    break;

                default:
                    abort();
            }
        }
    )

    return hasRoute;
}

static void _proc_trigger_termination(ProcessRef _Nonnull _Locked self, int signo)
{
    if (self->terminator_vcpu) {
        // termination is already in progress
        return;
    }

    self->signo_causing_termination = signo;
    self->terminator_vcpu = vcpu_from_owner_qe(self->vcpu_queue.first);

    vcpu_send_signal(self->terminator_vcpu, SIG_FORCE_QUIT);
    vcpu_resume(self->terminator_vcpu, true);
}

// This is how signals targeting a process are handled:
//
// SIG_FORCE_QUIT:   send to first vcpu in process. This vcpu becomes the termination coordinator
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
        case SIG_FORCE_QUIT:
            _proc_trigger_termination(self, signo);
            break;

        case SIG_FORCE_STOP:
            _proc_stop(self, WAIT_REASON_SIGNALED, signo, true);
            break;

        case SIG_CONTINUE:
            _proc_continue(self, WAIT_REASON_SIGNALED, signo, true);
            break;

        default:
            if (!_proc_route_signal(self, signo)) {
                switch (signo) {
                    case SIG_CPU_LIMIT:
                    case SIG_LOGOUT:
                    case SIG_QUIT:
                    case SIG_INTERRUPT:
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
                err = _proc_broadcast_signal_to_vcpu_group(self, vid, signo);
                break;

            case SIG_TARGET_VCPU_GROUP_ASAM:
                err = _proc_send_signal_to_vcpu_group_asam(self, vid, signo);
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
