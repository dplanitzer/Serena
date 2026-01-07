//
//  Process_Exit.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include <ext/timespec.h>
#include <hal/clock.h>
#include <hal/sched.h>
#include <kern/assert.h>
#include <kpi/wait.h>
#include <log/Log.h>

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
    switch (scope) {
        case JOIN_PROC:
            return ProcessManager_CopyZombieOfParent(gProcessManager, self->pid, id, pOutExists);

        case JOIN_PROC_GROUP:
            return ProcessManager_CopyGroupZombieOfParent(gProcessManager, self->pid, id, pOutExists);

        case JOIN_ANY:
            return ProcessManager_CopyAnyZombieOfParent(gProcessManager, self->pid, pOutExists);
    }
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

    ProcessManager_Unpublish(gProcessManager, zp);
    Process_Release(zp); // necessary because of the _find_matching_zombie() above

    return EOK;
}



// Force quit all child processes and reap their corpses. Do not return to the
// caller until all of them are dead and gone.
static void _proc_terminate_and_reap_children(ProcessRef _Nonnull self)
{
    sigcred_t sc;

    Process_GetSigcred(self, &sc);

    // Note that SIGCHILD is getting auto-routed to us (exit coordinator) because
    // the process is in exit state.
    ProcessManager_SendSignal(gProcessManager, &sc, SIG_SCOPE_PROC_CHILDREN, self->pid, SIGKILL);

    // Reap all zombies. There may have been zombies before we came here. That's
    // why we unconditionally execute this loop.
    for (;;) {
        struct proc_status ps;

        if (Process_TimedJoin(self, JOIN_ANY, 0, 0, &TIMESPEC_INF, &ps) == ECHILD) {
            break;
        }
    }
}

// Initiate an abort on every virtual processor attached to ourselves. Note that
// the VP that is running the process termination code has already taking itself
// out from the VP list.
void _proc_abort_other_vcpus(ProcessRef _Nonnull _Locked self)
{
    List_ForEach(&self->vcpu_queue, ListNode, {
        vcpu_t cvp = vcpu_from_owner_qe(pCurNode);

        vcpu_sigsend(cvp, SIGKILL);
    });
}

// Wait for all vcpus to relinquish themselves from the process. Only return
// once all vcpus are gone and no longer touch the process object.
void _proc_reap_vcpus(ProcessRef _Nonnull self)
{
    bool done = false;

    while (!done) {
        vcpu_yield();

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
        sigcred_t sc;

        Process_GetSigcred(self, &sc);
        ProcessManager_SendSignal(gProcessManager, &sc, SIG_SCOPE_PROC, self->ppid, SIGCHILD);
    }
}

// Zombify the process by freeing resources we no longer need at this point. The
// calling VP is the only one left touching the process. So this is safe.
void _proc_zombify(ProcessRef _Nonnull self)
{
    IOChannelTable_ReleaseAll(&self->ioChannelTable);
    AddressSpace_UnmapAll(&self->addr_space);
    FileManager_Deinit(&self->fm);

    self->state = PROC_STATE_ZOMBIE;
}

_Noreturn Process_Exit(ProcessRef _Nonnull self, int reason, int code)
{
    // We do not allow exiting the root process
    if (Process_IsRoot(self)) {
        abort();
    }


    mtx_lock(&self->mtx);
    const int isExitCoordinator = self->state < PROC_STATE_EXITING;
    
    if (isExitCoordinator) {
        self->state = PROC_STATE_EXITING;
        _proc_set_exit_reason(self, reason, code)
        self->exit_coordinator = vcpu_current();


        // This is the first vcpu going through the exit. It will act as the
        // termination/exit coordinator.
        // Take myself out from the vcpu list and send all other vcpus in the
        // process an abort signal.
        List_Remove(&self->vcpu_queue, &vcpu_current()->owner_qe);
        self->vcpu_count--;
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
    vcpu_relinquish(vcpu_current());
    /* NOT REACHED */
}
