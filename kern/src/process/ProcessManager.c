//
//  ProcessManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessManager.h"
#include "ProcessPriv.h"
#include <assert.h>
#include <ext/hash.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


#define proc_from_pid_qe(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(proc_rel_t, pid_qe))

#define proc_from_child_qe(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(proc_rel_t, child_qe))


#define HASH_CHAIN_COUNT    16
#define HASH_CHAIN_MASK     (HASH_CHAIN_COUNT - 1)


typedef struct ProcessManager {
    mtx_t                   mtx;
    pid_t                   next_pid;
    size_t                  proc_count;
    queue_t/*<Process>*/    pid_table[HASH_CHAIN_COUNT];     // pid_t -> Process
} ProcessManager;


ProcessManagerRef   gProcessManager;


errno_t ProcessManager_Create(ProcessManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessManagerRef self;
    
    try(kalloc_cleared(sizeof(ProcessManager), (void**) &self));
    
    mtx_init(&self->mtx);
    self->next_pid = 1;

catch:
    *pOutSelf = self;
    return EOK;
}

// Returns a weak reference to the process named by 'pid'. NULL if such a process
// does not exist.
static ProcessRef _Nullable _get_proc_by_pid(ProcessManagerRef _Nonnull _Locked self, pid_t pid)
{
    queue_for_each(&self->pid_table[hash_scalar(pid) & HASH_CHAIN_MASK], queue_node_t, it,
        ProcessRef cp = proc_from_pid_qe(it);

        if (cp->pid == pid) {
            return cp;
        }
    )

    return NULL;
}

static bool _has_group_leader(ProcessManagerRef _Nonnull _Locked self, pid_t pgrp)
{
    ProcessRef p = _get_proc_by_pid(self, pgrp);

    return (p->pgrp == pgrp) ? true : false;
}

static bool _has_session_leader(ProcessManagerRef _Nonnull _Locked self, pid_t sid)
{
    ProcessRef p = _get_proc_by_pid(self, sid);

    return (p->sid == sid) ? true : false;
}

static errno_t _publish_process(ProcessManagerRef _Nonnull _Locked self, ProcessRef _Nonnull pp)
{
    // Validate parameters:
    // pp->pid == 0
    // pp->ppid >= 1
    // pp->pgrp == 0 || group leader exists
    // pp->sid == 0 || session leader exists
    assert(pp->pid == 0);

    if (pp->pgrp != 0) {
        if (!_has_group_leader(self, pp->pgrp)) {
            return ESRCH;
        }
    }
    if (pp->sid != 0) {
        assert(_has_session_leader(self, pp->sid));
    }


    // Assign pid; pgrp and sid if needed
    pp->pid = self->next_pid++;
    if (pp->pgrp == 0) {
        pp->pgrp = pp->pid;
    }
    if (pp->sid == 0) {
        pp->sid = pp->pid;
    }
    

    // Add the new process to our relationship lists
    if (pp->pid != pp->ppid) {
        ProcessRef the_parent = _get_proc_by_pid(self, pp->ppid);
        assert(the_parent != NULL);

        queue_add_last(&the_parent->rel.children, &pp->rel.child_qe);
    }

    queue_add_first(&self->pid_table[hash_scalar(pp->pid) & HASH_CHAIN_MASK], &pp->rel.pid_qe);
    self->proc_count++;

    return EOK;
}

static void _unpublish_process(ProcessManagerRef _Nonnull _Locked self, ProcessRef _Nonnull pp)
{
    if (pp->pid == 0) {
        return;
    }

    ProcessRef the_parent = _get_proc_by_pid(self, pp->ppid);
    assert(the_parent != NULL);

    ProcessRef prev_child = NULL;
    queue_for_each(&the_parent->rel.children, queue_node_t, it,
        ProcessRef cp = proc_from_child_qe(it);

        if (cp->pid == pp->pid) {
            queue_remove(&the_parent->rel.children, &prev_child->rel.child_qe, &pp->rel.child_qe);
            break;
        }
        prev_child = cp;
    )

    
    ProcessRef prev_p = NULL;
    queue_t* pid_chain = &self->pid_table[hash_scalar(pp->pid) & HASH_CHAIN_MASK];
    queue_for_each(pid_chain, queue_node_t, it,
        ProcessRef cp = proc_from_pid_qe(it);

        if (cp->pid == pp->pid) {
            queue_remove(pid_chain, &prev_p->rel.pid_qe, &pp->rel.pid_qe);
            break;
        }
        prev_p = cp;
    )
    self->proc_count--;
}

errno_t ProcessManager_Publish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    err = _publish_process(self, pp);
    if (err == EOK) {
        // Take a strong reference out on the new process
        Process_Retain(pp);
    }
    mtx_unlock(&self->mtx);

    return err;
}

void ProcessManager_Unpublish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    mtx_lock(&self->mtx);
    _unpublish_process(self, pp);
    mtx_unlock(&self->mtx);

    // Drop our strong reference on the process
    Process_Release(pp);
}


ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull self, pid_t pid)
{
    mtx_lock(&self->mtx);

    ProcessRef the_p = _get_proc_by_pid(self, pid);
    if (the_p) {
        Process_Retain(the_p);
    }

    mtx_unlock(&self->mtx);

    return the_p;
}


struct matchres {
    ProcessRef _Nullable        p;
    proc_waitres_t* _Nonnull    r;
};

static errno_t _get_matching_state_of_process(ProcessManagerRef _Nonnull _Locked self, ProcessRef _Nonnull p, int mstate, struct matchres* _Nonnull res)
{
    const errno_t err = Process_GetMatchingState(p, mstate, res->r);

    if (err == EOK) {
        res->p = p;
    }
    return err;
}

static errno_t _get_status_for_pid_matching_state(ProcessManagerRef _Nonnull _Locked self, int mstate, pid_t ppid, pid_t pid, struct matchres* _Nonnull res)
{
    ProcessRef cp = _get_proc_by_pid(self, pid);

    if (cp && cp->ppid == ppid) {
        return _get_matching_state_of_process(self, cp, mstate, res);
    }

    return ECHILD;
}

static errno_t _get_status_for_pgrp_matching_state(ProcessManagerRef _Nonnull _Locked self, int mstate, pid_t ppid, pid_t pgrp, struct matchres* _Nonnull res)
{
    decl_try_err();
    ProcessRef pp = _get_proc_by_pid(self, ppid);
    size_t child_cnt = 0;

    if (pp == NULL) {
        return ESRCH;
    }

    queue_for_each(&pp->rel.children, queue_node_t, it,
        ProcessRef cp = proc_from_child_qe(it);

        if (cp->pgrp == pgrp) {
            child_cnt++;

            err = _get_matching_state_of_process(self, cp, mstate, res);
            if (err == EOK || err != EAGAIN) {
                return err;
            }
        }
    )

    return (child_cnt > 0) ? EAGAIN : ECHILD;
}

static errno_t _get_status_for_any_child_matching_state(ProcessManagerRef _Nonnull _Locked self, int mstate, pid_t ppid, struct matchres* _Nonnull res)
{
    ProcessRef pp = _get_proc_by_pid(self, ppid);

    if (pp == NULL) {
        return ESRCH;
    }
    if (queue_empty(&pp->rel.children)) {
        return ECHILD;
    }

    queue_for_each(&pp->rel.children, queue_node_t, it,
        ProcessRef cp = proc_from_child_qe(it);
        const errno_t err = _get_matching_state_of_process(self, cp, mstate, res);

        if (err == EOK || err != EAGAIN) {
            return err;
        }
    )

    return EAGAIN;
}

errno_t ProcessManager_GetStatusForProcessMatchingState(ProcessManagerRef _Nonnull self, int mstate, pid_t ppid, int match, pid_t id, proc_waitres_t* _Nonnull res)
{
    decl_try_err();
    struct matchres mr = {.p = NULL, .r = res};
    bool doRelease = false;

    switch (mstate) {
        case WAIT_FOR_ANY:
        case WAIT_FOR_RESUMED:
        case WAIT_FOR_SUSPENDED:
        case WAIT_FOR_TERMINATED:
            break;

        default:
            return EINVAL;
    }

    mtx_lock(&self->mtx);
    switch (match) {
        case WAIT_PID:
            err = _get_status_for_pid_matching_state(self, mstate, ppid, id, &mr);
            break;

        case WAIT_GROUP:
            err = _get_status_for_pgrp_matching_state(self, mstate, ppid, id, &mr);
            break;

        case WAIT_ANY:
            err = _get_status_for_any_child_matching_state(self, mstate, ppid, &mr);
            break;

        default:
            err = EINVAL;
            break;
    }

    // Reap the process if it has reached terminated state
    if (err == EOK && mr.p && mr.r->state == PROC_STATE_TERMINATED) {
        _unpublish_process(self, mr.p);
        doRelease = true;
    }

    mtx_unlock(&self->mtx);

    if (doRelease) {
        // Drop our strong reference on the process after dropping our lock
        Process_Release(mr.p);
    }

    return err;
}


errno_t ProcessManager_GetProcessIds(ProcessManagerRef _Nonnull self, pid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    decl_try_err();
    size_t idx = 0;

    // Min is 2 because there must be one process that is executing this code + the trailing 0
    if (bufSize < 2) {
        return ERANGE;
    }

    mtx_lock(&self->mtx);
    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        queue_for_each(&self->pid_table[i], queue_node_t, it,
            ProcessRef p = proc_from_pid_qe(it);

            buf[idx++] = Process_GetId(p);
            if (idx == bufSize-1) {
                i = HASH_CHAIN_COUNT;
                break;
            }
        )
    }

    buf[idx] = 0;
    *out_hasMore = (self->proc_count > idx) ? 1 : 0;
    mtx_unlock(&self->mtx);

    return err;
}


static errno_t _send_signal_to_proc(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    decl_try_err();
    ProcessRef target_p = _get_proc_by_pid(self, target_id);
    sigcred_t rcv;

    if (target_p == NULL) {
        return ESRCH;
    }

    Process_GetSigcred(target_p, &rcv);
    err = perm_check_send_signal(sndr, &rcv, signo);
    if (err == EOK) {
        err = Process_SendSignal(target_p, SIG_SCOPE_PROC, 0, signo);
    }

    return err;
}

static errno_t _send_signal_to_proc_children(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    decl_try_err();
    ProcessRef target_p = _get_proc_by_pid(self, target_id);
    sigcred_t rcv;
    bool hasSuccess = true;
    errno_t first_err = EOK;

    if (target_p == NULL || queue_empty(&target_p->rel.children)) {
        return ESRCH;
    }

    queue_for_each(&target_p->rel.children, queue_node_t, it,
        ProcessRef child_p = proc_from_child_qe(it);

        Process_GetSigcred(child_p, &rcv);
        err = perm_check_send_signal(sndr, &rcv, signo);
        if (err == EOK) {
            err = Process_SendSignal(child_p, SIG_SCOPE_PROC, 0, signo);
        }

        if (err == EOK) {
            hasSuccess = true;
        }
        else if (first_err == EOK) {
            first_err = err;
        }
    )

    if (hasSuccess) {
        return EOK;
    }
    else {
        return first_err;
    }
}

static errno_t _send_signal_to_proc_group(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    decl_try_err();
    sigcred_t rcv;
    bool hasMatch = false, hasSuccess = true;
    errno_t first_err = EOK;

    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        queue_for_each(&self->pid_table[i], queue_node_t, it,
            ProcessRef cp = proc_from_child_qe(it);

            if (cp->pgrp == target_id) {
                hasMatch = true;

                Process_GetSigcred(cp, &rcv);
                err = perm_check_send_signal(sndr, &rcv, signo);
                if (err == EOK) {
                    err = Process_SendSignal(cp, SIG_SCOPE_PROC, 0, signo);
                }

                if (err == EOK) {
                    hasSuccess = true;
                }
                else if (first_err == EOK) {
                    first_err = err;
                }
            }
        )
    }

    if (!hasMatch) {
        return ESRCH;
    }
    else if (hasSuccess) {
        return EOK;
    }
    else {
        return first_err;
    }
}

static errno_t _send_signal_to_session(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    decl_try_err();
    sigcred_t rcv;
    bool hasMatch = false, hasSuccess = true;
    errno_t first_err = EOK;

    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        queue_for_each(&self->pid_table[i], queue_node_t, it,
            ProcessRef cp = proc_from_child_qe(it);

            if (cp->sid == target_id) {
                hasMatch = true;

                Process_GetSigcred(cp, &rcv);
                err = perm_check_send_signal(sndr, &rcv, signo);
                if (err == EOK) {
                    err = Process_SendSignal(cp, SIG_SCOPE_PROC, 0, signo);
                }

                if (err == EOK) {
                    hasSuccess = true;
                }
                else if (first_err == EOK) {
                    first_err = err;
                }
            }
        )
    }

    if (!hasMatch) {
        return ESRCH;
    }
    else if (hasSuccess) {
        return EOK;
    }
    else {
        return first_err;
    }
}

errno_t ProcessManager_SendSignal(ProcessManagerRef _Nonnull self, const sigcred_t* _Nonnull sndr, int scope, id_t id, int signo)
{
    decl_try_err();

    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }
    if (signo == SIG_VCPU_RELINQUISH || signo == SIG_VCPU_SUSPEND) {
        return EPERM;
    }

    mtx_lock(&self->mtx);
    switch (scope) {
        case SIG_SCOPE_PROC:
            err = _send_signal_to_proc(self, sndr, id, signo);
            break;

        case SIG_SCOPE_PROC_CHILDREN:
            err = _send_signal_to_proc_children(self, sndr, id, signo);
            break;

        case SIG_SCOPE_PROC_GROUP:
            err = _send_signal_to_proc_group(self, sndr, id, signo);
            break;

        case SIG_SCOPE_SESSION:
            err = _send_signal_to_session(self, sndr, id, signo);
            break;

        default:
            err = EINVAL;
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}
