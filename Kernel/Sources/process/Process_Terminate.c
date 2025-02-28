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


// Frees all tombstones
void Process_DestroyAllTombstones_Locked(ProcessRef _Nonnull pProc)
{
    ProcessTombstone* pCurTombstone = (ProcessTombstone*)pProc->tombstones.first;

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
errno_t Process_OnChildTermination(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pChildProc)
{
    ProcessTombstone* pTombstone;
    const pid_t childPid = pChildProc->pid;
    const int childExitCode = pChildProc->exitCode;

    if (Process_IsTerminating(pProc)) {
        // We're terminating ourselves. Let the child know so that it should
        // bother someone else (session leader) with it's tombstone request.
        return ESRCH;
    }

    if (kalloc_cleared(sizeof(ProcessTombstone), (void**) &pTombstone) != EOK) {
        printf("Broken tombstone for %d:%d\n", pProc->pid, childPid);
        return EOK;
    }

    ListNode_Init(&pTombstone->node);
    pTombstone->pid = childPid;
    pTombstone->status = childExitCode;

    Lock_Lock(&pProc->lock);
    Process_AbandonChild_Locked(pProc, childPid);
    List_InsertAfterLast(&pProc->tombstones, &pTombstone->node);
    
    ConditionVariable_BroadcastAndUnlock(&pProc->tombstoneSignaler, &pProc->lock);
    
    if (pChildProc->terminationNotificationQueue) {
        DispatchQueue_DispatchAsync(pChildProc->terminationNotificationQueue,
            pChildProc->terminationNotificationClosure, pChildProc->terminationNotificationContext);
    }

    return EOK;
}

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if there are no tombstones of terminated
// child processes available or the PID is not the PID of a child process of
// the receiver. Otherwise blocks the caller until the requested process or any
// child process (pid == -1) has exited.
errno_t Process_WaitForTerminationOfChild(ProcessRef _Nonnull pProc, pid_t pid, ProcessTerminationStatus* _Nullable pStatus)
{
    decl_try_err();

    Lock_Lock(&pProc->lock);
    if (pid == -1 && List_IsEmpty(&pProc->tombstones)) {
        throw(ECHILD);
    }

    
    // Need to wait for a child to terminate
    while (true) {
        const ProcessTombstone* pTombstone = NULL;

        if (pid == -1) {
            // Any tombstone is good, return the first one (oldest) that was recorded
            pTombstone = (ProcessTombstone*)pProc->tombstones.first;
        } else {
            // Look for the specific child process
            List_ForEach(&pProc->tombstones, ProcessTombstone, {
                if (pCurNode->pid == pid) {
                    pTombstone = pCurNode;
                    break;
                }
            })

            if (pTombstone == NULL) {
                // Looks like the child isn't dead yet or 'pid' isn't referring to a child. Make sure it does
                if (!IntArray_Contains(&pProc->childPids, pid)) {
                    throw(ECHILD);
                }
            }
        }

        if (pTombstone) {
            if (pStatus) {
                pStatus->pid = pTombstone->pid;
                pStatus->status = pTombstone->status;
            }

            List_Remove(&pProc->tombstones, &pTombstone->node);
            kfree(pTombstone);
            break;
        }


        // Wait for a child to terminate
        try(ConditionVariable_Wait(&pProc->tombstoneSignaler, &pProc->lock, kTimeInterval_Infinity));
    }
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    Lock_Unlock(&pProc->lock);
    if (pStatus) {
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
static int Process_GetAnyChildPid(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const int pid = IntArray_GetFirst(&pProc->childPids, -1);
    Lock_Unlock(&pProc->lock);
    return pid;
}

// Runs on the kernel main dispatch queue and terminates the given process.
void _Process_DoTerminate(ProcessRef _Nonnull pProc)
{
    // Notes on terminating a process:
    //
    // All VPs belonging to a process are executing call-as-user invocations. The
    // first step of terminating a process is to abort all these invocations. This
    // is done by terminating all dispatch queues that belong to the process first.
    //
    // What does aborting a call-as-user invocation mean?
    // 1) If a VP is currently executing in user space then the user space
    //    invocation is aborted and the VP returns back to the dispatch queue
    //    main loop.
    // 2) If a VP is currently executing inside a system call then this system
    //    call has to first complete and we then abort the user space invocation
    //    that led to the system call when the system call would normally return
    //    to user space. So the return to user space is redirected to a piece of
    //    code that aborts the user space invocation. The VP then returns back
    //    to the dispatch queue main loop.
    // 3) A VP may be in waiting state because it executed a system call that
    //    invoked a blocking function. This wait will be interrupted/aborted as
    //    a side-effect of aborting the call-as-user invocation. Additionally all
    //    further abortable waits that the VP wants to take are immediately aborted
    //    until the VP has left the system call. This auto-abort does not apply
    //    to non-abortable waits like Lock_Lock.
    // 
    // Terminating a dispatch queue means that all queued up work items and timers
    // are flushed from the queue and that the queue relinquishes all its VPs. The
    // queue also stops accepting new work.
    //
    // A word on process termination and system calls:
    //
    // A system call MUST complete its run before the process data structures can
    // be freed. This is required because a system call manipulates kernel state
    // and we must ensure that every state manipulation is properly finalized
    // before we continue.
    // Note also that a system call that takes a kernel lock must eventually drop
    // this lock (it can not endlessly hold it) and it is expected to drop the
    // lock ASAP (it can not take unnecessarily long to release the lock). That's
    // why it is fine that Lock_Lock() is not interruptable even in the face of
    // the ability to terminate a process voluntarily/involuntarily.
    // The top-level system call handler checks whether a process is terminating
    // and it aborts the user space invocation that led to the system call. This
    // is the only required process termination check in a system call. All other
    // checks are voluntarily.
    // That said, every wait also does a check for process termination and the
    // wait immediately returns with an EINTR if the process is in the process of
    // being terminated. The only exception to this is the wait that Lock_Lock()
    // does since this kind of lock is a kernel lock that is used to preserve the
    // integrity of kernel data structures.

    // Notes on terminating a process tree:
    //
    // If a process terminates voluntarily or involuntarily then it'll by default
    // also terminate all its children, grand-children, etc processes. Every
    // process in the tree first terminates its children before it completes its
    // own termination. Doing it this way ensures that a parent process won't
    // (magically) disappear before all its children have terminated.


    // Terminate all dispatch queues. This takes care of aborting user space
    // invocations.
    DispatchQueue_Terminate(pProc->mainDispatchQueue);


    // Wait for all dispatch queues to have reached 'terminated' state
    DispatchQueue_WaitForTerminationCompleted(pProc->mainDispatchQueue);


    // Terminate all my children and wait for them to be dead
    while (true) {
        const pid_t pid = Process_GetAnyChildPid(pProc);

        if (pid <= 0) {
            break;
        }

        ProcessRef pCurChild = ProcessManager_CopyProcessForPid(gProcessManager, pid);
        
        Process_Terminate(pCurChild, 0);
        Process_WaitForTerminationOfChild(pProc, pid, NULL);
        Object_Release(pCurChild);
    }


    // Let our parent know that we're dead now and that it should remember us by
    // commissioning a beautiful tombstone for us.
    if (!Process_IsRoot(pProc)) {
        ProcessRef pParentProc = ProcessManager_CopyProcessForPid(gProcessManager, pProc->ppid);

        if (pParentProc) {
            if (Process_OnChildTermination(pParentProc, pProc) == ESRCH) {
                // XXXsession Try the session leader next. Give up if this fails too.
                // XXXsession. Unconditionally falling back to the root process for now.
                // Just drop the tombstone request if no one wants it.
                ProcessRef pRootProc = ProcessManager_CopyRootProcess(gProcessManager);
                Process_OnChildTermination(pRootProc, pProc);
                Object_Release(pRootProc);
            }
            Object_Release(pParentProc);
        }
    }


    // Finally destroy the process.
    ProcessManager_Unregister(gProcessManager, pProc);
    Object_Release(pProc);
}

// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
void Process_Terminate(ProcessRef _Nonnull pProc, int exitCode)
{
    // We do not allow exiting the root process
    if (Process_IsRoot(pProc)) {
        abort();
    }


    // Mark the process atomically as terminating. Leave now if some other VP
    // belonging to this process has already kicked off the termination. Note
    // that if multiple VPs concurrently execute a Process_Terminate(), that
    // at most one of them is able to get past this gate to kick off the
    // termination. All other VPs will return and their system calls will be
    // aborted. Also note that the Process data structure stays alive until
    // after _all_ VPs (including the first one) have returned from their
    // (aborted) system calls. So by the time the process data structure is
    // freed no system call that might directly or indirectly reference the
    // process is active anymore because all of them have been aborted and
    // unwound before we free the process data structure.
    if(AtomicBool_Set(&pProc->isTerminating, true)) {
        return;
    }


    // Remember the exit code
    pProc->exitCode = exitCode;


    // Schedule the actual process termination and destruction on the kernel
    // main dispatch queue.
    try_bang(DispatchQueue_DispatchAsync(ProcessManager_GetReaperQueue(gProcessManager), (VoidFunc_1) _Process_DoTerminate, pProc));
}

bool Process_IsTerminating(ProcessRef _Nonnull pProc)
{
    return pProc->isTerminating;
}
