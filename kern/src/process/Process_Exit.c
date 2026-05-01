//
//  Process_Exit.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"

// Operations that are mutual exclusive in the context of exiting a process:
// - exit
// - spawn child
// - exec
// - acquire vcpu
// - relinquish vcpu
// - distribute process level signal to vcpu(s)
// Exclusion is provided by 'mtx'. One important factor here is that exit and exec
// send SIG_TERMINATE to the vcpus in the process and they do this while holding 'mtx'.
// Thus operation like 'spawn child', 'acquire vcpu' should take 'mtx' and then
// check whether SIG_TERMINATE is pending. If it is return with EINTR since the vcpu is
// in the process of getting shot down.


// Force quit all child processes and reap their corpses. Do not return to the
// caller until all of them are dead and gone.
static void _proc_terminate_and_reap_children(ProcessRef _Nonnull self)
{
    sigcred_t sc;

    Process_GetSigcred(self, &sc);

    // Note that SIG_CHILD is getting auto-routed to us (exit coordinator) because
    // the process is in exit state.
    ProcessManager_SendSignal(gProcessManager, &sc, SIG_SCOPE_PROC_CHILDREN, self->pid, SIG_TERMINATE);

    // Reap all zombies. There may have been zombies before we came here. That's
    // why we unconditionally execute this loop.
    for (;;) {
        proc_waitres_t ps;

        if (Process_WaitForState(self, WAIT_FOR_TERMINATED, WAIT_ANY, 0, 0, &ps) == ECHILD) {
            break;
        }
    }
}

// Initiate an abort on every virtual processor attached to ourselves. Note that
// the VP that is running the process termination code has already taking itself
// out from the VP list.
void _proc_abort_other_vcpus(ProcessRef _Nonnull _Locked self)
{
    deque_for_each(&self->vcpu_queue, deque_node_t, it,
        vcpu_t cvp = vcpu_from_owner_qe(it);

        vcpu_send_signal(cvp, SIG_TERMINATE);
    )
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

// Zombify the process by freeing resources we no longer need at this point. The
// calling VP is the only one left touching the process. So this is safe.
static void _proc_zombify(ProcessRef _Nonnull self)
{
    IOChannelTable_ReleaseAll(&self->ioChannelTable);
    FileManager_Deinit(&self->fm);
    AddressSpace_UnmapAll(&self->addr_space);
}

_Noreturn void Process_Exit(ProcessRef _Nonnull self, int reason, int code)
{
    // We do not allow exiting the root process
    if (Process_IsRoot(self)) {
        abort();
    }


    mtx_lock(&self->mtx);
    const int isExitCoordinator = self->run_state < PROC_STATE_TERMINATING;
    
    if (isExitCoordinator) {
        _proc_set_exit_reason(self, reason, code)
        _proc_setstate(self, PROC_STATE_TERMINATING, false);
        self->exit_coordinator = vcpu_current();


        // This is the first vcpu going through the exit. It will act as the
        // termination/exit coordinator.
        // Take myself out from the vcpu list and send all other vcpus in the
        // process an abort signal.
        deque_remove(&self->vcpu_queue, &vcpu_current()->owner_qe);
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

    _proc_setstate(self, PROC_STATE_TERMINATED, true);

    
    // Finally relinquish myself
    vcpu_relinquish(vcpu_current());
    /* NOT REACHED */
}
