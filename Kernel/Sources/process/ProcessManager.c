//
//  ProcessManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessManager.h"
#include <dispatcher/Lock.h>
#include <kobj/ObjectArray.h>
#include "ProcessPriv.h"

typedef struct ProcessManager {
    Lock                    lock;
    ObjectArray             procs;      // XXX list vs hashtable (what we really want)
    ProcessRef _Nonnull     rootProc;
} ProcessManager;


ProcessManagerRef   gProcessManager;


// Creates the process manager. The provided process becomes the root process.
errno_t ProcessManager_Create(ProcessRef _Nonnull pRootProc, ProcessManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    ProcessManagerRef pManager;
    
    try_bang(kalloc(sizeof(ProcessManager), (void**) &pManager));
    Lock_Init(&pManager->lock);
    try_bang(ObjectArray_Init(&pManager->procs, 16));
    try_bang(ObjectArray_Add(&pManager->procs, (ObjectRef) pRootProc));
    pManager->rootProc = pRootProc;

    *pOutManager = pManager;
    return EOK;
}

// Returns a strong reference to the root process. This is the process that has
// no parent but all other processes are directly or indirectly descendants of
// the root process. The root process never changes identity and never goes
// away.
ProcessRef _Nonnull ProcessManager_CopyRootProcess(ProcessManagerRef _Nonnull pManager)
{
    // rootProc is a constant value, so no locking needed
    return Object_RetainAs(pManager->rootProc, Process);
}

// Looks up the process for the given PID. Returns NULL if no such process is
// registered with the process manager and otherwise returns a strong reference
// to the process object. The caller is responsible for releasing the reference
// once no longer needed.
ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull pManager, ProcessId pid)
{
    ProcessRef pProc = NULL;

    Lock_Lock(&pManager->lock);
    for (int i = 0; i < ObjectArray_GetCount(&pManager->procs); i++) {
        ProcessRef pCurProc = (ProcessRef) ObjectArray_GetAt(&pManager->procs, i);

        if (pCurProc->pid == pid) {
            pProc = Object_RetainAs(pCurProc, Process);
            break;
        }
    }
    Lock_Unlock(&pManager->lock);
    return pProc;
}

// Registers the given process with the process manager. Note that this function
// does not validate whether the process is already registered or has a PID
// that's equal to some other registered process.
// A process will only become visible to other processes after it has been
// registered with the process manager. 
errno_t ProcessManager_Register(ProcessManagerRef _Nonnull pManager, ProcessRef _Nonnull pProc)
{
    decl_try_err();

    Lock_Lock(&pManager->lock);
    err = ObjectArray_Add(&pManager->procs, (ObjectRef) pProc);
    Lock_Unlock(&pManager->lock);
    return err;
}

// Deregisters the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// registered.
void ProcessManager_Unregister(ProcessManagerRef _Nonnull pManager, ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pManager->lock);
    assert(pProc != pManager->rootProc);
    ObjectArray_RemoveIdenticalTo(&pManager->procs, (ObjectRef) pProc);
    Lock_Unlock(&pManager->lock);
}
