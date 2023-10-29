//
//  ProcessManager.c
//  Apollo
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessManager.h"
#include "Lock.h"
#include "ProcessPriv.h"

typedef struct _ProcessManager {
    Lock                    lock;
    ProcessRef* _Nonnull    procs;      // XXX revisit dynamic array vs list vs hashtable (what we really want)
    ProcessRef _Nonnull     rootProc;
} ProcessManager;
#define PROC_CAPACITY   16


ProcessManagerRef   gProcessManager;


// Creates the process manager. The provided process becomes the root process.
ErrorCode ProcessManager_Create(ProcessRef _Nonnull pRootProc, ProcessManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    ProcessManagerRef pManager;
    
    try_bang(kalloc(sizeof(ProcessManager), (Byte**)&pManager));
    Lock_Init(&pManager->lock);
    try_bang(kalloc_cleared(sizeof(ProcessRef) * PROC_CAPACITY, (Byte**)&pManager->procs));
    pManager->procs[PROC_CAPACITY - 1] = Object_RetainAs(pRootProc, Process);
    pManager->rootProc = pRootProc;

    *pOutManager = pManager;
    return EOK;
}

// Returns a strong reference to the root process. This is the process that has
// no parent but all other processes are directly or indirectly descandants of
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
ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull pManager, Int pid)
{
    ProcessRef pProc = NULL;

    Lock_Lock(&pManager->lock);
    for (Int i = 0; i < PROC_CAPACITY; i++) {
        ProcessRef pCurProc = pManager->procs[i];

        if (pCurProc && pCurProc->pid == pid) {
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
void ProcessManager_Register(ProcessManagerRef _Nonnull pManager, ProcessRef _Nonnull pProc)
{
    Bool okay = false;

    Lock_Lock(&pManager->lock);
    for (Int i = 0; i < PROC_CAPACITY; i++) {
        if (pManager->procs[i] == NULL) {
            pManager->procs[i] = Object_RetainAs(pProc, Process);
            okay = true;
            break;
        }
    }
    assert(okay);
    Lock_Unlock(&pManager->lock);
}

// Deregisters the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// registered.
void ProcessManager_Unregister(ProcessManagerRef _Nonnull pManager, ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pManager->lock);
    assert(pProc != pManager->rootProc);
    for (Int i = 0; i < PROC_CAPACITY; i++) {
        if (pManager->procs[i] == pProc) {
            Object_Release(pManager->procs[i]);
            pManager->procs[i] = NULL;
            break;
        }
    }
    Lock_Unlock(&pManager->lock);
}
