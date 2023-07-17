//
//  Process.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Process.h"
#include "DispatchQueue.h"
#include "SystemGlobals.h"


typedef struct _Process {
    Int                 pid;
    DispatchQueueRef    mainDispatchQueue;
} Process;


// Returns the next PID available for use by a new process.
Int Process_GetNextAvailablePID(void)
{
    SystemGlobals* pGlobals = SystemGlobals_Get();

    return AtomicInt_Increment(&pGlobals->next_available_pid);
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
    try(DispatchQueue_Create(1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRIORITY_NORMAL, pProc, &pProc->mainDispatchQueue));

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

// Finalizes the destruction of the given process. This is done on the kernel
// main queue to ensure that no virtual processor is actively using the process
// object anymore.
static void Process_ExitCompletion(ProcessRef _Nonnull pProc)
{
    Process_Destroy(pProc);
}

// Continues the process exit sequence by enqueuing the final exit stage on the
// kernel main dispatch queue. The trampoline is executed right after we have
// aborted the user space invocation that triggered the process exit. This
// function is the last function that executes on any of the process dispatch
// queues.
static void Process_ExitTrampoline(ProcessRef _Nonnull pProc)
{
    // Bounce over to the kernel main queue to destroy the process.
    assert(DispatchQueue_DispatchAsync(DispatchQueue_GetMain(), DispatchQueueClosure_Make((Closure1Arg_Func) Process_ExitCompletion, (Byte*) pProc)) == EOK);
}

// Voluntarily exits the given process. Assumes that this function is called from
// a system call originating in user space and that this call is made from one of
// the dispatch queue execution contexts owned by the process.
void Process_Exit(ProcessRef _Nonnull pProc)
{
    DispatchQueueRef pQueue = DispatchQueue_GetCurrent();
    assert(pProc == DispatchQueue_GetOwningProcess(pQueue));


    // We do not allow exiting the root process
    if (pProc == SystemGlobals_Get()->root_process) {
        abort();
    }


    // Get rid of all queued up work items and timers.
    assert(DispatchQueue_Flush(pQueue) == EOK);


    // Enqueue a trampoline that will get us over to the kernel main queue which
    // is where we are going to destroy the process. We do things this way because
    // it gives us the guarantee that the destruction of the process object will
    // only start once all dispatch queues owned by the process have stopped doing
    // work and we have aborted the user space invocation that got us here.
    assert(DispatchQueue_DispatchAsync(pQueue, DispatchQueueClosure_Make((Closure1Arg_Func) Process_ExitTrampoline, (Byte*) pProc)) == EOK);


    // Abort the user space invocation. This will cause us to drop back into the
    // main loop of the dispatch queue that orignally initiated the call-as-user
    // (userspace) invocation that ultimately got us here. This in turn will then
    // cause this dispatch queue to execute the Process_ExitTrampoline.
    assert(VirtualProcessor_AbortCallAsUser(VirtualProcessor_GetCurrent()) == EOK);
}