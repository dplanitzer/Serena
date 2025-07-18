//
//  Process_Terminate.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include <dispatcher/VirtualProcessorPool.h>
#include <kern/timespec.h>
#include <log/Log.h>

extern void cpu_abort_vcpu_from_uspace(void);


// Called by the given child process to notify its parent about its death.
static errno_t _proc_on_child_termination(ProcessRef _Nonnull self, ProcessRef _Nonnull child)
{
    if (self->state >= PS_ZOMBIFYING) {
        return ESRCH;
    }


    Lock_Lock(&self->lock);
    ConditionVariable_Broadcast(&self->procTermSignaler);
    Lock_Unlock(&self->lock);

    return EOK;
}

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if the child process 'pid' is still active.
// Otherwise blocks the caller until the requested process or any child process
// (pid == -1) has exited.
errno_t Process_WaitForTerminationOfChild(ProcessRef _Nonnull self, pid_t pid, struct _pstatus* _Nonnull pStatus, int options)
{
    decl_try_err();
    ProcessRef zp = NULL;

    Lock_Lock(&self->lock);
    for (;;) {
        bool hasChildWithPid = false;

        List_ForEach(&self->children, ListNode, {
            ProcessRef cpp = proc_from_siblings(pCurNode);

            if (pid == -1 || cpp->pid == pid) {
                hasChildWithPid = true;

                if (cpp->state == PS_ZOMBIE) {
                    zp = cpp;
                    List_Remove(&self->children, &zp->siblings);
                    break;
                }
            }
        });

        if (!hasChildWithPid) {
            err = ECHILD;
            break;
        }

        if (zp) {
            break;
        }

        if ((options & WNOHANG) == WNOHANG) {
            err = ECHILD;
            break;
        }


        // Wait for a child to terminate
        err = ConditionVariable_Wait(&self->procTermSignaler, &self->lock);
        if (err != EOK) {
            break;
        }
    }
    Lock_Unlock(&self->lock);


    if (zp) {
        pStatus->pid = zp->pid;
        pStatus->status = zp->exitCode;

        ProcessManager_Deregister(gProcessManager, zp);
        Object_Release(zp);
    }
    else {
        pStatus->pid = 0;
        pStatus->status = 0;
    }

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
    Lock_Lock(&self->lock);
    List_ForEach(&self->vpQueue, ListNode, {
        VirtualProcessor* cvp = VP_FROM_OWNER_NODE(pCurNode);

        const int sps = preempt_disable();
        VirtualProcessor_Signal(cvp, SIGKILL);
        preempt_restore(sps);
    });
    Lock_Unlock(&self->lock);
}

// Wait for all vcpus to relinquish themselves from the process. Only return
// once all vcpus are gone and no longer touch the process object.
static WaitQueue gHackQueue;
static void _proc_reap_vcpus(ProcessRef _Nonnull self)
{
    bool done = false;

    while (!done) {
#if 1
        //XXX Can't use yield() here because the currently implemented scheduler
        //XXX algorithm is too weak and ends up starving some vps
        //VirtualProcessor_Yield();
        struct timespec delay;
        timespec_from_ms(&delay, 10);
        WaitQueue_TimedWait(&gHackQueue, NULL, 0, &delay, NULL);

        Lock_Lock(&self->lock);
        if (self->vpQueue.first == NULL) {
            done = true;
        }
        Lock_Unlock(&self->lock);
#else
        Lock_Lock(&self->lock);
        if (self->vpQueue.first) {
            List_ForEach(&self->vpQueue, ListNode, {
                VirtualProcessor* cvp = VP_FROM_OWNER_NODE(pCurNode);

                VirtualProcessor_Suspend(cvp);

               // printf("sr: %x, vp: %p\n", (int)cvp->save_area.sr, cvp);
                if ((cvp->save_area.sr & 0x2000) == 0 && (cvp->flags & VP_FLAG_ABORTED_USPACE) == 0) {
                    printf("caught in uspace -> %p\n", cvp);
                    // User space:
                    // redirect the VP to the new call
                    cvp->save_area.pc = (uint32_t)cpu_abort_vcpu_from_uspace;
                    cvp->flags |= VP_FLAG_ABORTED_USPACE;
                }

                VirtualProcessor_Resume(cvp, true);
            });
        }
        else {
            done = true;
        }
        Lock_Unlock(&self->lock);
#endif
    }
}

// Let our parent know that we're dead now and that it should remember us by
// commissioning a beautiful tombstone for us.
static void _proc_notify_parent(ProcessRef _Nonnull self)
{
    if (!Process_IsRoot(self)) {
        ProcessRef pParentProc = ProcessManager_CopyProcessForPid(gProcessManager, self->ppid);

        if (pParentProc) {
            if (_proc_on_child_termination(pParentProc, self) == ESRCH) {
                // XXXsession Try the session leader next. Give up if this fails too.
                // XXXsession. Unconditionally falling back to the root process for now.
                // Just drop the tombstone request if no one wants it.
                ProcessRef pRootProc = ProcessManager_CopyRootProcess(gProcessManager);
                _proc_on_child_termination(pRootProc, self);
                Object_Release(pRootProc);
            }
            Object_Release(pParentProc);
        }
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


    if (isExiting) {
        return;
    }


    _proc_terminate_and_reap_children(self);

    _proc_detach_calling_vcpu(self);
    _proc_abort_vcpus(self);
    _proc_reap_vcpus(self);

    _proc_zombify(self);

    _proc_notify_parent(self);

    
    // Finally relinquish myself
    VirtualProcessorPool_RelinquishVirtualProcessor(
        gVirtualProcessorPool,
        VirtualProcessor_GetCurrent());
    /* NOT REACHED */
}
