//
//  Process.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Process.h"
#include "Atomic.h"
#include "kalloc.h"
#include "DispatchQueue.h"


typedef struct _Process {
    Int                 pid;
    DispatchQueueRef    mainDispatchQueue;
    AtomicBool          isTerminating;  // true if the process is going through the termination process
} Process;


ProcessRef  gRootProcess;


// Returns the next PID available for use by a new process.
Int Process_GetNextAvailablePID(void)
{
    static volatile AtomicInt gNextAvailablePid = 0;
    return AtomicInt_Increment(&gNextAvailablePid);
}

// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
ProcessRef _Nullable Process_GetCurrent(void)
{
    DispatchQueueRef pQueue = DispatchQueue_GetCurrent();

    return (pQueue != NULL) ? DispatchQueue_GetOwningProcess(pQueue) : NULL;
}



ErrorCode Process_Create(Int pid, ProcessRef _Nullable * _Nonnull pOutProc)
{
    decl_try_err();
    Process* pProc;
    
    try(kalloc_cleared(sizeof(Process), (Byte**) &pProc));
    
    pProc->pid = pid;
    try(DispatchQueue_Create(1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRIORITY_NORMAL, gVirtualProcessorPool, pProc, &pProc->mainDispatchQueue));

    *pOutProc = pProc;
    return EOK;

catch:
    Process_Destroy(pProc);
    *pOutProc = NULL;
    return err;
}

void Process_Destroy(ProcessRef _Nullable pProc)
{
    if (pProc) {
        DispatchQueue_Destroy(pProc->mainDispatchQueue);
        pProc->mainDispatchQueue = NULL;

        kfree((Byte*) pProc);
    }
}

Int Process_GetPID(ProcessRef _Nonnull pProc)
{
    return pProc->pid;
}

ErrorCode Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure)
{
    return DispatchQueue_DispatchAsync(pProc->mainDispatchQueue, DispatchQueueClosure_MakeUser(pUserClosure, NULL));
}

// Runs on the kernel main dispatch queue and terminates the given process.
static void _Process_DoTerminate(ProcessRef _Nonnull pProc)
{
    // Mark the process atomically as terminating
    if(AtomicBool_Set(&pProc->isTerminating, true)) {
        return;
    }


    // XXX hack for now because we are not currently aborting waits properly
    VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(10));


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
    // 3) A VP may be in waiting state because it was executing a system call that
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
    // lock ASAP (it can not take unecessarily long to release the lock). That's
    // why it is fine that Lock_Lock() is not interruptable even in the face of
    // the ability to terminate a process voluntarily/involuntarily.
    // The top-level system call handler checks whether a process is terminating
    // and it aborts the user space invocation that led to the system call. This
    // is the only required process termination check in a system call. All other
    // checks are voluntarily.
    // That said, every wait also does a check for process termination and the
    // wait immediatly returns with an EINTR if the process is in the process of
    // being terminated. The only exception to this is the wait that Lock_Lock()
    // does since this kind of lock is a kernel lock that is used to preserve the
    // integrity of kernel data structures.


    // Terminate all dispatch queues. This takes care of aborting user space
    // invocations.
    //print("a\n");
    DispatchQueue_Terminate(pProc->mainDispatchQueue);


    // Wait for all dispatch queues to have reached 'terminated' state
    DispatchQueue_WaitForTerminationCompleted(pProc->mainDispatchQueue);


    // Finally destroy the process.
    //print("b\n");
    Process_Destroy(pProc);

    //print("DONE\n");
}

// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously.
void Process_Terminate(ProcessRef _Nonnull pProc)
{
    // We do not allow exiting the root process
    if (pProc == gRootProcess) {
        abort();
    }


    // Schedule the actual process termination and destruction on the kernel
    // main dispatch queue.
    try_bang(DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func) _Process_DoTerminate, (Byte*) pProc)));
}

Bool Process_IsTerminating(ProcessRef _Nonnull pProc)
{
    return pProc->isTerminating;
}