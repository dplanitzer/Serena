//
//  ProcessManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessManager.h"
#include <dispatcher/Lock.h>
#include <klib/Hash.h>
#include "ProcessPriv.h"
#include <kern/kalloc.h>


#define HASH_CHAIN_COUNT    16
#define HASH_CHAIN_MASK     (HASH_CHAIN_COUNT - 1)


typedef struct ProcessManager {
    Lock                        lock;
    ProcessRef _Nonnull         rootProc;
    List/*<Process>*/           procTable[HASH_CHAIN_COUNT];     // pid_t -> Process
} ProcessManager;


ProcessManagerRef   gProcessManager;


// Creates the process manager. The provided process becomes the root process.
errno_t ProcessManager_Create(ProcessRef _Nonnull pRootProc, ProcessManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessManagerRef self;
    
    try_bang(kalloc(sizeof(ProcessManager), (void**) &self));
    
    Lock_Init(&self->lock);
    
    for (int i = 0; i < HASH_CHAIN_COUNT; i++) {
        List_Init(&self->procTable[i]);
    }

    self->rootProc = pRootProc;
    ProcessManager_Register(self, pRootProc);

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
    ProcessRef proc = NULL;

    Lock_Lock(&self->lock);
    List_ForEach(&self->procTable[hash_scalar(pid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef pCurProc = proc_from_ptce(pCurNode);

        if (pCurProc->pid == pid) {
            proc = Object_RetainAs(pCurProc, Process);
            break;
        }
    );
    Lock_Unlock(&self->lock);

    return proc;
}

// Registers the given process with the process manager. Note that this function
// does not validate whether the process is already registered or has a PID
// that's equal to some other registered process.
// A process will only become visible to other processes after it has been
// registered with the process manager. 
void ProcessManager_Register(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pProc)
{
    Lock_Lock(&self->lock);
    List_InsertBeforeFirst(&self->procTable[hash_scalar(pProc->pid) & HASH_CHAIN_MASK], &pProc->ptce);
    Object_Retain(pProc);
    Lock_Unlock(&self->lock);
}

// Deregisters the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// registered.
void ProcessManager_Deregister(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pProc)
{
    Lock_Lock(&self->lock);
    assert(pProc != self->rootProc);
    List_Remove(&self->procTable[hash_scalar(pProc->pid) & HASH_CHAIN_MASK], &pProc->ptce);
    Object_Release(pProc);
    Lock_Unlock(&self->lock);
}
