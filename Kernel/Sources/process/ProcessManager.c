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
    mtx_t               mtx;
    List/*<Process>*/   procTable[HASH_CHAIN_COUNT];     // pid_t -> Process
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

    err = ProcessManager_Register(self, pRootProc);

catch:
    *pOutSelf = self;
    return EOK;
}

// Returns a weak reference to the process named by 'pid'. NULL if such a process
// does not exist.
static ProcessRef _Nullable _get_proc_by_pid(ProcessManagerRef _Nonnull _Locked self, pid_t pid)
{
    List_ForEach(&self->procTable[hash_scalar(pid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef cp = proc_from_ptce(pCurNode);

        if (cp->pid == pid) {
            return cp;
        }
    );

    return NULL;
}

errno_t ProcessManager_Register(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    err = Process_Publish(pp);
    if (err == EOK) {
        if (pp->pid != pp->ppid) {
            ProcessRef the_parent = _get_proc_by_pid(self, pp->ppid);
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
    assert(pp->pid != PID_KERNEL);

    mtx_lock(&self->mtx);
    Process_Unpublish(pp);
    ProcessRef the_parent = _get_proc_by_pid(self, pp->ppid);
    assert(the_parent != NULL);

    List_Remove(&the_parent->children, &pp->siblings);
    List_Remove(&self->procTable[hash_scalar(pp->pid) & HASH_CHAIN_MASK], &pp->ptce);
    mtx_unlock(&self->mtx);

    Object_Release(pp);
}


ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull self, pid_t pid)
{
    mtx_lock(&self->mtx);

    ProcessRef the_p = _get_proc_by_pid(self, pid);
    if (the_p) {
        Object_Retain(the_p);
    }

    mtx_unlock(&self->mtx);

    return the_p;
}

ProcessRef _Nullable ProcessManager_CopyZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, pid_t pid, bool* _Nonnull pOutExists)
{
    mtx_lock(&self->mtx);
    ProcessRef child_p = _get_proc_by_pid(self, pid);
    ProcessRef the_p = NULL;

    if (child_p && child_p->ppid == ppid) {
        *pOutExists = true;

        if (Process_GetLifecycleState(child_p) == PROC_LIFECYCLE_ZOMBIE) {
            the_p = Object_RetainAs(child_p, Process);
        }
    }
    else {
        *pOutExists = false;
    }

    mtx_unlock(&self->mtx);

    return the_p;
}

static ProcessRef _Nullable _get_any_zombie_of_parent(ProcessManagerRef _Nonnull _Locked self, pid_t ppid, pid_t pgrp, bool* _Nonnull pOutAnyExists)
{
    ProcessRef parent_p = _get_proc_by_pid(self, ppid);

    *pOutAnyExists = false;
    if (parent_p) {
        List_ForEach(&parent_p->children, ListNode,
            ProcessRef child_p = proc_from_siblings(pCurNode);

            if (pgrp == 0 || (pgrp > 0 && child_p->pgrp == pgrp)) {
                *pOutAnyExists = true;

                if (Process_GetLifecycleState(child_p) == PROC_LIFECYCLE_ZOMBIE) {
                    return child_p;
                }
            }
        );
    }

    return NULL;
}

ProcessRef _Nullable ProcessManager_CopyGroupZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, pid_t pgrp, bool* _Nonnull pOutAnyExists)
{
    mtx_lock(&self->mtx);

    ProcessRef the_p = _get_any_zombie_of_parent(self, ppid, pgrp, pOutAnyExists);
    if (the_p) {
        Object_Retain(the_p);
    }

    mtx_unlock(&self->mtx);

    return the_p;
}

ProcessRef _Nullable ProcessManager_CopyAnyZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, bool* _Nonnull pOutAnyExists)
{
    mtx_lock(&self->mtx);

    ProcessRef the_p = _get_any_zombie_of_parent(self, ppid, 0, pOutAnyExists);
    if (the_p) {
        Object_Retain(the_p);
    }

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
