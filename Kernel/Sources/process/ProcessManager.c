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

errno_t ProcessManager_Register(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    decl_try_err();
    ProcessRef the_parent = NULL;

    mtx_lock(&self->mtx);
    err = Process_Publish(pp);
    if (err == EOK) {
        if (pp->pid != pp->ppid) {
            List_ForEach(&self->procTable[hash_scalar(pp->ppid) & HASH_CHAIN_MASK], ListNode,
                ProcessRef parent_p = proc_from_ptce(pCurNode);

                if (parent_p->pid == pp->ppid) {
                    the_parent = parent_p;
                    break;
                }
            );
            assert(the_parent != NULL);

            List_InsertAfterLast(&the_parent->children, &pp->siblings);
        }

        List_InsertBeforeFirst(&self->procTable[hash_scalar(pp->pid) & HASH_CHAIN_MASK], &pp->ptce);
        Object_Retain(pp);
    }
    mtx_unlock(&self->mtx);

    return err;
}

void ProcessManager_Deregister(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    ProcessRef the_parent = NULL;

    mtx_lock(&self->mtx);
    assert(pp != self->rootProc);

    Process_Unpublish(pp);
    List_ForEach(&self->procTable[hash_scalar(pp->ppid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef parent_p = proc_from_ptce(pCurNode);

        if (parent_p->pid == pp->ppid) {
            the_parent = parent_p;
            break;
        }
    );
    assert(the_parent != NULL);

    List_Remove(&the_parent->children, &pp->siblings);
    List_Remove(&self->procTable[hash_scalar(pp->pid) & HASH_CHAIN_MASK], &pp->ptce);
    Object_Release(pp);
    mtx_unlock(&self->mtx);
}


ProcessRef _Nonnull ProcessManager_CopyRootProcess(ProcessManagerRef _Nonnull self)
{
    // rootProc is a constant value, so no locking needed
    return Object_RetainAs(self->rootProc, Process);
}

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

ProcessRef _Nullable ProcessManager_CopyZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, pid_t pid, bool* _Nonnull pOutExists)
{
    ProcessRef the_p = NULL;

    *pOutExists = false;

    mtx_lock(&self->mtx);
    List_ForEach(&self->procTable[hash_scalar(pid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef child_p = proc_from_ptce(pCurNode);

        if (child_p->pid == pid && child_p->ppid == ppid) {
            *pOutExists = true;

            if (Process_GetLifecycleState(child_p) == PROC_LIFECYCLE_ZOMBIE) {
                the_p = Object_RetainAs(child_p, Process);
            }
            break;
        }
    );
    mtx_unlock(&self->mtx);

    return the_p;
}

ProcessRef _Nullable ProcessManager_CopyGroupZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, pid_t pgrp, bool* _Nonnull pOutAnyExists)
{
    ProcessRef the_p = NULL;

    *pOutAnyExists = false;

    mtx_lock(&self->mtx);
    List_ForEach(&self->procTable[hash_scalar(ppid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef parent_p = proc_from_ptce(pCurNode);

        if (parent_p->pid == ppid) {
            List_ForEach(&parent_p->children, ListNode,
                ProcessRef child_p = proc_from_siblings(pCurNode);

                if (child_p->pgrp == pgrp) {
                    *pOutAnyExists = true;

                    if (Process_GetLifecycleState(child_p) == PROC_LIFECYCLE_ZOMBIE) {
                        the_p = Object_RetainAs(child_p, Process);
                        break;
                    }
                }
            );

            break;
        }
    );
    mtx_unlock(&self->mtx);

    return the_p;
}

ProcessRef _Nullable ProcessManager_CopyAnyZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, bool* _Nonnull pOutAnyExists)
{
    ProcessRef the_p = NULL;

    *pOutAnyExists = false;

    mtx_lock(&self->mtx);
    List_ForEach(&self->procTable[hash_scalar(ppid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef parent_p = proc_from_ptce(pCurNode);

        if (parent_p->pid == ppid) {
            List_ForEach(&parent_p->children, ListNode,
                ProcessRef child_p = proc_from_siblings(pCurNode);

                *pOutAnyExists = true;
                if (Process_GetLifecycleState(child_p) == PROC_LIFECYCLE_ZOMBIE) {
                    the_p = Object_RetainAs(child_p, Process);
                    break;
                }

            );

            break;
        }
    );
    mtx_unlock(&self->mtx);

    return the_p;
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
