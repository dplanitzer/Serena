//
//  Process_Terminate.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"

// Operations that are mutual exclusive in the context of terminating a process:
// - exit
// - spawn child
// - exec
// - acquire vcpu
// - relinquish vcpu
// - distribute process level signal to vcpu(s)
// Exclusion is provided by 'mtx'. One important factor here is that exit and exec
// send SIG_FORCE_QUIT to the vcpus in the process and they do this while holding 'mtx'.
// Thus operation like 'spawn child', 'acquire vcpu' should take 'mtx' and then
// check whether SIG_FORCE_QUIT is pending. If it is return with ECANCELED since the vcpu is
// in the process of getting shot down.
//
// Possible termination paths:
//
// exit(): calls Process_Terminate() directly and the exiting vcpu becomes the
//          termination coordinator.
//
// SIG_FORCE_QUIT: if sent to the process, it triggers process termination by
//          making the first vcpu in the process the termination coordinator.
//          The process then forwards the SIG_FORCE_QUIT to this vcpu which then
//          calls Process_Terminate() from its syscall bottom half.
//
// proc_terminate() system call: simply sends a SIG_FORCE_QUIT to the process.
//

// Force quit all child processes and reap their corpses. Do not return to the
// caller until all of them are dead and gone.
static void _proc_terminate_and_reap_children(ProcessRef _Nonnull self)
{
    // Note that SIG_CHILD is getting auto-routed to us (exit coordinator) because
    // the process is in exit state.
    Process_SendSignal(self, SIG_TARGET_PROC_CHILDREN, self->pid, 0, SIG_FORCE_QUIT);

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

        vcpu_send_signal(cvp, SIG_FORCE_QUIT);
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

bool _proc_is_terminating(ProcessRef _Nonnull self)
{
    return ((self->flags & PROC_FLAG_TERMINATING) == PROC_FLAG_TERMINATING) ? true : false;
}

_Noreturn void Process_Terminate(ProcessRef _Nonnull self, int reason, int arg)
{
    // We do not allow terminating the root process
    if (self->pid == PID_KERNELD) {
        abort();
    }

    // the vcpu executing this code must belong to the process we're trying to
    // terminate
    if (vcpu_current()->proc != self) {
        abort();
    }


    mtx_lock(&self->mtx);
    if (_proc_is_terminating(self)) {
        mtx_unlock(&self->mtx);
        // We're in the process of terminating the process and this isn't the
        // first vcpu calling Process_Terminate(). Just relinquish at this point
        // since this is what we're expected to do anyway.
        Process_RelinquishVirtualProcessor(self, vcpu_current());
        /* NOT REACHED */
        return;
    }


    // This is the first vcpu going through Process_Terminate(). It will act as
    // the process terminator.
    // Take it out from the vcpu list and tell all other vcpus in the process
    // to relinquish.
    self->flags |= PROC_FLAG_TERMINATING;
    self->terminator_vcpu = vcpu_current();

    deque_remove(&self->vcpu_queue, &vcpu_current()->owner_qe);
    self->vcpu_count--;
    _proc_abort_other_vcpus(self);
    mtx_unlock(&self->mtx);


    _proc_reap_vcpus(self);

    _proc_terminate_and_reap_children(self);
    _proc_zombify(self);

    _proc_set_state(self, PROC_STATE_TERMINATED, reason, arg, true);

    
    // Finally relinquish myself
    vcpu_relinquish(vcpu_current());
    /* NOT REACHED */
}
