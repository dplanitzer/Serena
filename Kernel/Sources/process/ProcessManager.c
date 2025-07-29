//
//  ProcessManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessManager.h"
#include <klib/Hash.h>
#include "ProcessPriv.h"
#include <kern/kalloc.h>
#include <sched/mtx.h>


#define HASH_CHAIN_COUNT    16
#define HASH_CHAIN_MASK     (HASH_CHAIN_COUNT - 1)


typedef struct ProcessManager {
    mtx_t                       mtx;
    ProcessRef _Nonnull         rootProc;
    List/*<Process>*/           procTable[HASH_CHAIN_COUNT];     // pid_t -> Process
} ProcessManager;


ProcessManagerRef   gProcessManager;


// Creates the process manager. The provided process becomes the root process.
errno_t ProcessManager_Create(ProcessRef _Nonnull pRootProc, ProcessManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessManagerRef self;
    
    try(kalloc(sizeof(ProcessManager), (void**) &self));
    
    mtx_init(&self->mtx);
    
    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        List_Init(&self->procTable[i]);
    }

    self->rootProc = pRootProc;
    err = ProcessManager_Register(self, pRootProc);

catch:
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

    mtx_lock(&self->mtx);
    List_ForEach(&self->procTable[hash_scalar(pid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef pCurProc = proc_from_ptce(pCurNode);

        if (pCurProc->pid == pid) {
            proc = Object_RetainAs(pCurProc, Process);
            break;
        }
    );
    mtx_unlock(&self->mtx);

    return proc;
}

// Registers the given process with the process manager. Note that this function
// does not validate whether the process is already registered or has a PID
// that's equal to some other registered process.
// A process will only become visible to other processes after it has been
// registered with the process manager. 
errno_t ProcessManager_Register(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    err = Process_Publish(pp);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->procTable[hash_scalar(pp->pid) & HASH_CHAIN_MASK], &pp->ptce);
        Object_Retain(pp);
    }
    mtx_unlock(&self->mtx);

    return err;
}

// Deregisters the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// registered.
void ProcessManager_Deregister(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    mtx_lock(&self->mtx);
    assert(pp != self->rootProc);

    Process_Unpublish(pp);
    List_Remove(&self->procTable[hash_scalar(pp->pid) & HASH_CHAIN_MASK], &pp->ptce);
    Object_Release(pp);
    mtx_unlock(&self->mtx);
}

errno_t ProcessManager_SendSignal(ProcessManagerRef _Nonnull self, id_t sender_sid, int scope, id_t id, int signo)
{
    decl_try_err();
    bool hasMatched = false;

    mtx_lock(&self->mtx);
    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        List_ForEach(&self->procTable[i], ListNode,
            ProcessRef cp = proc_from_ptce(pCurNode);
            int doSend;

            switch (scope) {
                case SIG_SCOPE_PROC:
                    doSend = (id == cp->pid);
                    break;

                case SIG_SCOPE_PROC_CHILDREN:
                    doSend = (id == cp->ppid);
                    break;

                case SIG_SCOPE_PROC_GROUP:
                    doSend = (id == cp->pgrp);
                    break;

                case SIG_SCOPE_SESSION:
                    doSend = (id == cp->sid);
                    break;

                default:
                    abort();
            }

            if (doSend) {
                if (sender_sid == cp->sid) {
                   err = Process_SendSignal(cp, SIG_SCOPE_PROC, 0, signo);
                }
                else {
                    err = EPERM;
                }

                if (err != EOK) {
                    break;
                }
                hasMatched = true;
            }
        );

        if (err != EOK) {
            break;
        }
    }
    mtx_unlock(&self->mtx);

    if (err == EOK && !hasMatched) {
        err = ESRCH;
    }

    return err;
}
