//
//  ProcessManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessManager.h"
#include "ProcessPriv.h"
#include <Catalog.h>
#include <ext/hash.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


#define proc_from_pid_qe(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(proc_rel_t, pid_qe))

#define proc_from_child_qe(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(proc_rel_t, child_qe))


#define HASH_CHAIN_COUNT    16
#define HASH_CHAIN_MASK     (HASH_CHAIN_COUNT - 1)


typedef struct ProcessManager {
    mtx_t               mtx;
    pid_t               nextPid;
    SList/*<Process>*/  pid_table[HASH_CHAIN_COUNT];     // pid_t -> Process
} ProcessManager;


ProcessManagerRef   gProcessManager;


errno_t ProcessManager_Create(ProcessManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessManagerRef self;
    
    try(kalloc_cleared(sizeof(ProcessManager), (void**) &self));
    
    mtx_init(&self->mtx);
    self->nextPid = 1;

catch:
    *pOutSelf = self;
    return EOK;
}

FilesystemRef _Nonnull ProcessManager_GetCatalog(ProcessManagerRef _Nonnull self)
{
    return Catalog_GetFilesystem(gProcCatalog);
}

// Returns a weak reference to the process named by 'pid'. NULL if such a process
// does not exist.
static ProcessRef _Nullable _get_proc_by_pid(ProcessManagerRef _Nonnull _Locked self, pid_t pid)
{
    SList_ForEach(&self->pid_table[hash_scalar(pid) & HASH_CHAIN_MASK], ListNode,
        ProcessRef cp = proc_from_pid_qe(pCurNode);

        if (cp->pid == pid) {
            return cp;
        }
    );

    return NULL;
}

errno_t ProcessManager_Publish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    decl_try_err();
    static char g_pid_buf[DIGIT_BUFFER_CAPACITY];

    mtx_lock(&self->mtx);

    if (pp->pid != 0) {
        throw(EBUSY);
    }

    pp->pid = self->nextPid++;
    if (pp->pgrp == 0) {
        pp->pgrp = pp->pid;
    }
    if (pp->sid == 0) {
        pp->sid = pp->pid;
    }
    
    UInt32_ToString(pp->pid, 10, false, g_pid_buf);
    try(Catalog_PublishProcess(gProcCatalog, g_pid_buf, kUserId_Root, kGroupId_Root, perm_from_octal(0444), pp, &pp->rel.cat_id));
    
    if (pp->pid != pp->ppid) {
        ProcessRef the_parent = _get_proc_by_pid(self, pp->ppid);
        assert(the_parent != NULL);

        SList_InsertAfterLast(&the_parent->rel.children, &pp->rel.child_qe);
    }

    SList_InsertBeforeFirst(&self->pid_table[hash_scalar(pp->pid) & HASH_CHAIN_MASK], &pp->rel.pid_qe);
    Process_Retain(pp);

catch:
    mtx_unlock(&self->mtx);

    return err;
}

void ProcessManager_Unpublish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp)
{
    mtx_lock(&self->mtx);

    if (pp->pid == 0) {
        mtx_unlock(&self->mtx);
        return;
    }

    assert(pp->rel.cat_id != kCatalogId_None);

    Catalog_Unpublish(gProcCatalog, kCatalogId_None, pp->rel.cat_id);
    pp->rel.cat_id = kCatalogId_None;


    ProcessRef the_parent = _get_proc_by_pid(self, pp->ppid);
    assert(the_parent != NULL);

    ProcessRef prev_child = NULL;
    SList_ForEach(&the_parent->rel.children, ListNode,
        ProcessRef cp = proc_from_child_qe(pCurNode);

        if (cp->pid == pp->pid) {
            SList_Remove(&the_parent->rel.children, &prev_child->rel.child_qe, &pp->rel.child_qe);
            break;
        }
        prev_child = cp;
    );

    
    ProcessRef prev_p = NULL;
    SList* pid_chain = &self->pid_table[hash_scalar(pp->pid) & HASH_CHAIN_MASK];
    SList_ForEach(pid_chain, ListNode,
        ProcessRef cp = proc_from_pid_qe(pCurNode);

        if (cp->pid == pp->pid) {
            SList_Remove(pid_chain, &prev_p->rel.pid_qe, &pp->rel.pid_qe);
            break;
        }
        prev_p = cp;
    );

    mtx_unlock(&self->mtx);
    Process_Release(pp);
}


ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull self, pid_t pid)
{
    mtx_lock(&self->mtx);

    ProcessRef the_p = _get_proc_by_pid(self, pid);
    if (the_p) {
        Process_Retain(the_p);
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

        if (Process_GetInexactState(child_p) == PROC_STATE_ZOMBIE) {
            the_p = Process_Retain(child_p);
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
        SList_ForEach(&parent_p->rel.children, ListNode,
            ProcessRef child_p = proc_from_child_qe(pCurNode);

            if (pgrp == 0 || (pgrp > 0 && child_p->pgrp == pgrp)) {
                *pOutAnyExists = true;

                if (Process_GetInexactState(child_p) == PROC_STATE_ZOMBIE) {
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
        Process_Retain(the_p);
    }

    mtx_unlock(&self->mtx);

    return the_p;
}

ProcessRef _Nullable ProcessManager_CopyAnyZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, bool* _Nonnull pOutAnyExists)
{
    mtx_lock(&self->mtx);

    ProcessRef the_p = _get_any_zombie_of_parent(self, ppid, 0, pOutAnyExists);
    if (the_p) {
        Process_Retain(the_p);
    }

    mtx_unlock(&self->mtx);

    return the_p;
}


static errno_t _send_signal_to_proc(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    ProcessRef target_p = _get_proc_by_pid(self, target_id);
    sigcred_t rcv;

    if (target_p == NULL) {
        return ESRCH;
    }

    Process_GetSigcred(target_p, &rcv);
    if (SecurityManager_CanSendSignal(gSecurityManager, sndr, &rcv, signo)) {
        return Process_SendSignal(target_p, SIG_SCOPE_PROC, 0, signo);
    }
    else {
        return EPERM;
    }
}

static errno_t _send_signal_to_proc_children(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    decl_try_err();
    ProcessRef target_p = _get_proc_by_pid(self, target_id);
    sigcred_t rcv;
    bool hasChildren = false, hasSuccess = true;
    errno_t first_err = EOK;

    if (target_p == NULL) {
        return ESRCH;
    }

    SList_ForEach(&target_p->rel.children, ListNode, 
        ProcessRef child_p = proc_from_child_qe(pCurNode);

        hasChildren = true;
        Process_GetSigcred(child_p, &rcv);
        if (SecurityManager_CanSendSignal(gSecurityManager, sndr, &rcv, signo)) {
            err = Process_SendSignal(child_p, SIG_SCOPE_PROC, 0, signo);
            if (err == EOK) {
                hasSuccess = true;
            }
            else if (first_err != EOK) {
                first_err = err;
            }
        }
    );

    if (!hasChildren) {
        return ESRCH;
    }
    else if (hasSuccess) {
        return EOK;
    }
    else {
        return first_err;
    }
}

static errno_t _send_signal_to_proc_group(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    decl_try_err();
    sigcred_t rcv;
    bool hasMatch = false, hasSuccess = true;
    errno_t first_err = EOK;

    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        SList_ForEach(&self->pid_table[i], ListNode, 
            ProcessRef cp = proc_from_child_qe(pCurNode);

            if (cp->pgrp == target_id) {
                hasMatch = true;

                Process_GetSigcred(cp, &rcv);
                if (SecurityManager_CanSendSignal(gSecurityManager, sndr, &rcv, signo)) {
                    err = Process_SendSignal(cp, SIG_SCOPE_PROC, 0, signo);
                    if (err == EOK) {
                        hasSuccess = true;
                    }
                    else if (first_err != EOK) {
                        first_err = err;
                    }
                }
            }
        );
    }

    if (!hasMatch) {
        return ESRCH;
    }
    else if (hasSuccess) {
        return EOK;
    }
    else {
        return first_err;
    }
}

static errno_t _send_signal_to_session(ProcessManagerRef _Nonnull _Locked self, const sigcred_t* _Nonnull sndr, id_t target_id, int signo)
{
    decl_try_err();
    sigcred_t rcv;
    bool hasMatch = false, hasSuccess = true;
    errno_t first_err = EOK;

    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        SList_ForEach(&self->pid_table[i], ListNode, 
            ProcessRef cp = proc_from_child_qe(pCurNode);

            if (cp->sid == target_id) {
                hasMatch = true;

                Process_GetSigcred(cp, &rcv);
                if (SecurityManager_CanSendSignal(gSecurityManager, sndr, &rcv, signo)) {
                    err = Process_SendSignal(cp, SIG_SCOPE_PROC, 0, signo);
                    if (err == EOK) {
                        hasSuccess = true;
                    }
                    else if (first_err != EOK) {
                        first_err = err;
                    }
                }
            }
        );
    }

    if (!hasMatch) {
        return ESRCH;
    }
    else if (hasSuccess) {
        return EOK;
    }
    else {
        return first_err;
    }
}

errno_t ProcessManager_SendSignal(ProcessManagerRef _Nonnull self, const sigcred_t* _Nonnull sndr, int scope, id_t id, int signo)
{
    decl_try_err();

    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }
    if (signo == SIGVPRQ || signo == SIGVPDS) {
        return EPERM;
    }

    mtx_lock(&self->mtx);
    switch (scope) {
        case SIG_SCOPE_PROC:
            err = _send_signal_to_proc(self, sndr, id, signo);
            break;

        case SIG_SCOPE_PROC_CHILDREN:
            err = _send_signal_to_proc_children(self, sndr, id, signo);
            break;

        case SIG_SCOPE_PROC_GROUP:
            err = _send_signal_to_proc_group(self, sndr, id, signo);
            break;

        case SIG_SCOPE_SESSION:
            err = _send_signal_to_session(self, sndr, id, signo);
            break;

        default:
            err = EINVAL;
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}
