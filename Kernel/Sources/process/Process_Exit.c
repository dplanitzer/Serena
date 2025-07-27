//
//  Process_Exit.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include <kern/timespec.h>
#include <kpi/wait.h>
#include <log/Log.h>
#include <machine/clock.h>
#include <machine/csw.h>
#include <sched/vcpu_pool.h>

// Operations that are mutual exclusive in the context of exiting a process:
// - exit
// - spawn child
// - exec
// - acquire vcpu
// - relinquish vcpu
// - distribute process level signal to vcpu(s)
// Exclusion is provided by 'mtx'. One important factor here is that exit and exec
// send SIGKILL to the vcpus in the process and they do this while holding 'mtx'.
// Thus operation like 'spawn child', 'acquire vcpu' should take 'mtx' and then
// check whether SIGKILL is pending. If it is return with EINTR since the vcpu is
// in the process of getting shot down.

static ProcessRef _Nullable _find_matching_zombie(ProcessRef _Nonnull self, int scope, pid_t id, bool* _Nonnull pOutExists)
{
    List_ForEach(&self->children, ListNode,
        ProcessRef cpp = proc_from_siblings(pCurNode);
        int hasMatch;

        *pOutExists = false;
        switch (scope) {
            case JOIN_PROC:
                if (cpp->pid == id) {
                    *pOutExists = true;
                    return (cpp->state == PS_ZOMBIE) ? cpp : NULL;
                }
                hasMatch = 0;
                break;

            case JOIN_PROC_GROUP:
                hasMatch = cpp->pgrp == id;
                break;

            case JOIN_ANY:
                hasMatch = 1;
                break;
        }

        if (hasMatch) {
            *pOutExists = true;
            if (cpp->state == PS_ZOMBIE) {
                return cpp;
            }
        }
    );

    return NULL;
}

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if the function was told to wait for a
// specific process or process group and the process or group does not exist.
errno_t Process_TimedJoin(ProcessRef _Nonnull self, int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps)
{
    decl_try_err();
    ProcessRef zp = NULL;
    struct timespec deadline;
    bool exists = false;
    sigset_t hot_sigs = _SIGBIT(SIGCHILD);
    int signo;

    switch (scope) {
        case JOIN_PROC:
        case JOIN_PROC_GROUP:
        case JOIN_ANY:
            break;

        default:
            return EINVAL;
    }


    if ((flags & TIMER_ABSTIME) == 0) {
        struct timespec now;

        clock_gettime(g_mono_clock, &now);
        timespec_add(&now, wtp, &deadline);
    }
    else {
        deadline = *wtp;
    }


    for (;;) {
        mtx_lock(&self->mtx);

        zp = _find_matching_zombie(self, scope, id, &exists);
        if (zp) {
            List_Remove(&self->children, &zp->siblings);
            mtx_unlock(&self->mtx);
            break;
        }

        if (!exists) {
            mtx_unlock(&self->mtx);
            return ECHILD;
        }

        if (timespec_eq(wtp, &TIMESPEC_ZERO)) {
            mtx_unlock(&self->mtx);
            return ETIMEDOUT;
        }

        mtx_unlock(&self->mtx);
        

        err = vcpu_sigtimedwait(&self->siwa_queue, &hot_sigs, flags | TIMER_ABSTIME, &deadline, &signo);
        if (err != EOK) {
            return err;
        }
    }


    ps->pid = zp->pid;
    ps->reason = zp->exit_reason;
    ps->u.status = zp->exit_code;

    ProcessManager_Deregister(gProcessManager, zp);
    Object_Release(zp);

    return EOK;
}

// Returns the PID of *any* of the receiver's children. This is used by the
// termination code to terminate all children. We don't care about the order
// in which we terminate the children but we do care that we trigger the
// termination of all of them. Keep in mind that a child may itself trigger its
// termination concurrently with our termination. The process is inherently
// racy and thus we need to be defensive about things. Returns 0 if there are
// no more children.
static int Process_GetAnyChildPid(ProcessRef _Nonnull self)
{
    int pid = -1;

    mtx_lock(&self->mtx);
    if (self->children.first) {
        ProcessRef pChildProc = proc_from_siblings(self->children.first);

        pid = pChildProc->pid;
    }
    mtx_unlock(&self->mtx);
    return pid;
}



// Force quit all child processes and reap their corpses. Do not return to the
// caller until all of them are dead and gone.
static void _proc_terminate_and_reap_children(ProcessRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int hasChildren = self->children.first != NULL;
    mtx_unlock(&self->mtx);


    if (hasChildren) {
        vcpu_sigroute(vcpu_current(), SIG_ROUTE_ENABLE);
        ProcessManager_SendSignal(gProcessManager, self->sid, SIG_SCOPE_PROC_CHILDREN, self->pid, SIGKILL);

        for (;;) {
            struct proc_status ps;

            if (Process_TimedJoin(self, JOIN_ANY, 0, 0, &TIMESPEC_INF, &ps) == ECHILD) {
                break;
            }
        }
        vcpu_sigrouteoff(vcpu_current());
    }
}

// Initiate an abort on every virtual processor attached to ourselves. Note that
// the VP that is running the process termination code has already taking itself
// out from the VP list.
static void _proc_abort_other_vcpus(ProcessRef _Nonnull _Locked self)
{
    List_ForEach(&self->vcpu_queue, ListNode, {
        vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);

        vcpu_sigsend(cvp, SIGKILL, false);
        vcpu_sigrouteoff(cvp);
    });
}

// Wait for all vcpus to relinquish themselves from the process. Only return
// once all vcpus are gone and no longer touch the process object.
static struct waitqueue gHackQueue;
static void _proc_reap_vcpus(ProcessRef _Nonnull self)
{
    bool done = false;

    while (!done) {
        //XXX Can't use yield() here because the currently implemented scheduler
        //XXX algorithm is too weak and ends up starving some vps
        //vcpu_yield();
        struct timespec delay;
        timespec_from_ms(&delay, 10);
        wq_timedwait(&gHackQueue, NULL, 0, &delay, NULL);

        mtx_lock(&self->mtx);
        if (self->vcpu_queue.first == NULL) {
            done = true;
        }
        mtx_unlock(&self->mtx);
    }
}

// Let our parent know that we're dead now and that it should remember us by
// commissioning a beautiful tombstone for us.
static void _proc_notify_parent(ProcessRef _Nonnull self)
{
    if (!Process_IsRoot(self)) {
        ProcessManager_SendSignal(gProcessManager, self->sid, SIG_SCOPE_PROC, self->ppid, SIGCHILD);
    }
}

// Zombify the process by freeing resources we no longer need at this point. The
// calling VP is the only one left touching the process. So this is safe.
void _proc_zombify(ProcessRef _Nonnull self)
{
    IOChannelTable_ReleaseAll(&self->ioChannelTable);
    AddressSpace_UnmapAll(self->addressSpace);
    FileManager_Deinit(&self->fm);

    self->state = PS_ZOMBIE;
}

_Noreturn Process_Exit(ProcessRef _Nonnull self, int reason, int code)
{
    // We do not allow exiting the root process
    if (Process_IsRoot(self)) {
        abort();
    }


    mtx_lock(&self->mtx);
    const int isExitCoordinator = self->state < PS_ZOMBIFYING;
    
    if (isExitCoordinator) {
        self->state = PS_ZOMBIFYING;
        self->exit_reason = reason;
        self->exit_code = code;


        // This is the first vcpu going through the exit. It will act as the
        // termination/exit coordinator.
        // Take myself out from the vcpu list and send all other vcpus in the
        // process an abort signal.
        List_Remove(&self->vcpu_queue, &vcpu_current()->owner_qe);
        _proc_abort_other_vcpus(self);
    }
    mtx_unlock(&self->mtx);


    if (!isExitCoordinator) {
        // This is any of the other vcpus in the process that we are shutting
        // down. Just relinquish yourself. The exit coordinator is blocked
        // waiting for all the other vcpus to relinquish before it will proceed
        // with the process zombification.
        Process_RelinquishVirtualProcessor(self, vcpu_current());
        /* NOT REACHED */
    }


    _proc_reap_vcpus(self);

    _proc_terminate_and_reap_children(self);
    _proc_zombify(self);

    _proc_notify_parent(self);

    
    // Finally relinquish myself
    vcpu_pool_relinquish(g_vcpu_pool, vcpu_current());
    /* NOT REACHED */
}
