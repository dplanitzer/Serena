//
//  Process_Terminate.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include <log/Log.h>
#include <kern/kalloc.h>
#include <kern/timespec.h>
#include <dispatchqueue/DispatchQueue.h>


// Frees all tombstones
void Process_DestroyAllTombstones_Locked(ProcessRef _Nonnull self)
{
    ProcessTombstone* pCurTombstone = (ProcessTombstone*)self->tombstones.first;

    while (pCurTombstone) {
        ProcessTombstone* nxt = (ProcessTombstone*)pCurTombstone->node.next;

        kfree(pCurTombstone);
        pCurTombstone = nxt;
    }
}

// Called by the given child process tp notify its parent about its death.
// Creates a new tombstone for the given child process with the given exit status
// and posts a termination notification closure if one was provided for the child
// process. Expects that the child process state does not change while this function
// is executing.
errno_t Process_OnChildTermination(ProcessRef _Nonnull self, ProcessRef _Nonnull child)
{
    ProcessTombstone* pTombstone;

    if (self->state >= PS_ZOMBIFYING) {
        // We're terminating ourselves. Let the child know so that it should
        // bother someone else (session leader) with it's tombstone request.
        return ESRCH;
    }

    if (kalloc_cleared(sizeof(ProcessTombstone), (void**) &pTombstone) != EOK) {
        printf("Broken tombstone for %d:%d\n", self->pid, child->pid);
        return EOK;
    }

    ListNode_Init(&pTombstone->node);
    pTombstone->pid = child->pid;
    pTombstone->status = child->exitCode;

    Lock_Lock(&self->lock);
    Process_AbandonChild_Locked(self, child);
    List_InsertAfterLast(&self->tombstones, &pTombstone->node);
    
    ConditionVariable_Broadcast(&self->tombstoneSignaler);
    Lock_Unlock(&self->lock);

    return EOK;
}

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if there are no tombstones of terminated
// child processes available or the PID is not the PID of a child process of
// the receiver. Otherwise blocks the caller until the requested process or any
// child process (pid == -1) has exited.
errno_t Process_WaitForTerminationOfChild(ProcessRef _Nonnull self, pid_t pid, struct _pstatus* _Nonnull pStatus, int options)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (pid == -1 && List_IsEmpty(&self->tombstones)) {
        throw(ECHILD);
    }

    
    // Need to wait for a child to terminate
    while (true) {
        const ProcessTombstone* pTombstone = NULL;

        if (pid == -1) {
            // Any tombstone is good, return the first one (oldest) that was recorded
            pTombstone = (ProcessTombstone*)self->tombstones.first;
        } else {
            // Look for the specific child process
            List_ForEach(&self->tombstones, ProcessTombstone, {
                if (pCurNode->pid == pid) {
                    pTombstone = pCurNode;
                    break;
                }
            })

            if (pTombstone == NULL) {
                // Looks like the child isn't dead yet or 'pid' isn't referring to a child. Make sure it does
                bool hasChild = false;

                List_ForEach(&self->children, ListNode,
                    ProcessRef pCurProc = proc_from_siblings(pCurNode);

                    if (pCurProc->pid == pid) {
                        hasChild = true;
                        break;
                    }
                );
                if (!hasChild) {
                    throw(ECHILD);
                }
            }
        }

        if (pTombstone) {
            pStatus->pid = pTombstone->pid;
            pStatus->status = pTombstone->status;

            List_Remove(&self->tombstones, &pTombstone->node);
            kfree(pTombstone);
            break;
        }

        if ((options & WNOHANG) == WNOHANG) {
            pStatus->pid = 0;
            pStatus->status = 0;
            break;
        }


        // Wait for a child to terminate
        try(ConditionVariable_Wait(&self->tombstoneSignaler, &self->lock));
    }

catch:
    Lock_Unlock(&self->lock);
    return err;
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

    Lock_Lock(&self->lock);
    if (self->children.first) {
        ProcessRef pChildProc = proc_from_siblings(self->children.first);

        pid = pChildProc->pid;
    }
    Lock_Unlock(&self->lock);
    return pid;
}



// Force quit all child processes and reap their corpses. Do not return to the
// caller until all of them are dead and gone.
static void _proc_terminate_and_reap_children(ProcessRef _Nonnull self)
{
    while (true) {
        const pid_t pid = Process_GetAnyChildPid(self);

        if (pid <= 0) {
            break;
        }

        ProcessRef pCurChild = ProcessManager_CopyProcessForPid(gProcessManager, pid);
        
        Process_Terminate(pCurChild, 0);
        Process_WaitForTerminationOfChild(self, pid, NULL, 0);
        Object_Release(pCurChild);
    }
}

// Take the calling VP out from the process' VP list since it will relinquish
// itself at the very end of the process termination sequence.
static void _proc_detach_calling_vcpu(ProcessRef _Nonnull self)
{
    Process_DetachVirtualProcessor(self, VirtualProcessor_GetCurrent());
}

// Initiate an abort on every virtual processor attached to ourselves. Note that
// the VP that is running the process termination code has already taking itself
// out from the VP list.
static void _proc_abort_vcpus(ProcessRef _Nonnull self)
{

}

// Wait for all vcpus to relinquish themselves from the process. Only return
// once all vcpus are gone and no longer touch the process object.
static void _proc_reap_vcpus(ProcessRef _Nonnull self)
{

}

// Let our parent know that we're dead now and that it should remember us by
// commissioning a beautiful tombstone for us.
static void _proc_notify_parent(ProcessRef _Nonnull self)
{
    if (!Process_IsRoot(self)) {
        ProcessRef pParentProc = ProcessManager_CopyProcessForPid(gProcessManager, self->ppid);

        if (pParentProc) {
            if (Process_OnChildTermination(pParentProc, self) == ESRCH) {
                // XXXsession Try the session leader next. Give up if this fails too.
                // XXXsession. Unconditionally falling back to the root process for now.
                // Just drop the tombstone request if no one wants it.
                ProcessRef pRootProc = ProcessManager_CopyRootProcess(gProcessManager);
                Process_OnChildTermination(pRootProc, self);
                Object_Release(pRootProc);
            }
            Object_Release(pParentProc);
        }
    }
}

// Zombify the process by freeing resources we no longer need at this point. The
// calling VP is the only one left touching the process. So this is safe.
void Process_Zombify(ProcessRef _Nonnull self)
{
    self->state = PS_ZOMBIE;
}

// Runs on the kernel main dispatch queue and terminates the given process.
static _Noreturn _proc_terminate(ProcessRef _Nonnull self)
{
    _proc_terminate_and_reap_children(self);

    _proc_detach_calling_vcpu(self);
    _proc_abort_vcpus(self);
    _proc_reap_vcpus(self);

    Process_Zombify(self);

    _proc_notify_parent(self);


    // Destroy the process.
    ProcessManager_Deregister(gProcessManager, self);
    Object_Release(self);

    // Finally relinquish myself
    VirtualProcessorPool_RelinquishVirtualProcessor(
        gVirtualProcessorPool,
        VirtualProcessor_GetCurrent());
    /* NOT REACHED */
}

// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
void Process_Terminate(ProcessRef _Nonnull self, int exitCode)
{
    // We do not allow exiting the root process
    if (Process_IsRoot(self)) {
        abort();
    }


    Lock_Lock(&self->lock);
    const int isExiting = self->state >= PS_ZOMBIFYING;
    
    if (!isExiting) {
        self->state = PS_ZOMBIFYING;
        self->exitCode = exitCode & _WSTATUSMASK;
    }
    Lock_Unlock(&self->lock);


    if (!isExiting) {
        _proc_terminate(self);
    }
}
