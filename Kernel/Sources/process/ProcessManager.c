//
//  ProcessManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessManager.h"
#include <dispatcher/Lock.h>
#include <dispatchqueue/DispatchQueue.h>
#include <kobj/ObjectArray.h>
#include "ProcessPriv.h"

typedef struct ProcessManager {
    Lock                        lock;
    ObjectArray                 procs;      // XXX list vs hashtable (what we really want)
    ProcessRef _Nonnull         rootProc;
    DispatchQueueRef _Nonnull   reaperQueue;
} ProcessManager;


ProcessManagerRef   gProcessManager;


// Creates the process manager. The provided process becomes the root process.
errno_t ProcessManager_Create(ProcessRef _Nonnull pRootProc, ProcessManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessManagerRef self;
    
    try_bang(kalloc(sizeof(ProcessManager), (void**) &self));
    Lock_Init(&self->lock);
    try_bang(ObjectArray_Init(&self->procs, 16));
    try_bang(ObjectArray_Add(&self->procs, (ObjectRef) pRootProc));
    self->rootProc = pRootProc;
    try_bang(DispatchQueue_Create(1, 1, kDispatchQoS_Realtime, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->reaperQueue));

    *pOutSelf = self;
    return EOK;
}

// Returns a strong reference to the root process. This is the process that has
// no parent but all other processes are directly or indirectly descendants of
// the root process. The root process never changes identity and never goes
// away.
ProcessRef _Nonnull ProcessManager_CopyRootProcess(ProcessManagerRef _Nonnull self)
{
    // rootProc is a constant value, so no locking needed
    return Object_RetainAs(self->rootProc, Process);
}

// Looks up the process for the given PID. Returns NULL if no such process is
// registered with the process manager and otherwise returns a strong reference
// to the process object. The caller is responsible for releasing the reference
// once no longer needed.
ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull self, pid_t pid)
{
    ProcessRef pProc = NULL;

    Lock_Lock(&self->lock);
    for (int i = 0; i < ObjectArray_GetCount(&self->procs); i++) {
        ProcessRef pCurProc = (ProcessRef) ObjectArray_GetAt(&self->procs, i);

        if (pCurProc->pid == pid) {
            pProc = Object_RetainAs(pCurProc, Process);
            break;
        }
    }
    Lock_Unlock(&self->lock);
    return pProc;
}

// Registers the given process with the process manager. Note that this function
// does not validate whether the process is already registered or has a PID
// that's equal to some other registered process.
// A process will only become visible to other processes after it has been
// registered with the process manager. 
errno_t ProcessManager_Register(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pProc)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = ObjectArray_Add(&self->procs, (ObjectRef) pProc);
    Lock_Unlock(&self->lock);
    return err;
}

// Deregisters the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// registered.
void ProcessManager_Unregister(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pProc)
{
    Lock_Lock(&self->lock);
    assert(pProc != self->rootProc);
    ObjectArray_RemoveIdenticalTo(&self->procs, (ObjectRef) pProc);
    Lock_Unlock(&self->lock);
}

// Returns the process reaper queue.
DispatchQueueRef _Nonnull ProcessManager_GetReaperQueue(ProcessManagerRef _Nonnull self)
{
    return self->reaperQueue;
}
