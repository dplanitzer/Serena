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



ProcessRef _Nullable Process_Create(Int pid)
{
    Process* pProc = (Process*)kalloc(sizeof(Process), HEAP_ALLOC_OPTION_CLEAR);
    FailNULL(pProc);
    
    pProc->pid = pid;
    pProc->mainDispatchQueue = DispatchQueue_Create(1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRIORITY_NORMAL, pProc);
    FailNULL(pProc->mainDispatchQueue);

    return pProc;

failed:
    Process_Destroy(pProc);
    return NULL;
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

void Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure)
{
    DispatchQueue_DispatchAsync(pProc->mainDispatchQueue, DispatchQueueClosure_MakeUser(pUserClosure, NULL));
}
