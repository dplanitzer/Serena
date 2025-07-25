//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filemanager/FileHierarchy.h>
#include <sched/vcpu_pool.h>


class_func_defs(Process, Object,
override_func_def(deinit, Process, Object)
);


errno_t RootProcess_Create(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutSelf)
{
    InodeRef rootDir = FileHierarchy_AcquireRootDirectory(pRootFh);
    const errno_t err = Process_Create(1, 1, 1, 1, pRootFh, kUserId_Root, kGroupId_Root, rootDir, rootDir, perm_from_octal(0022), pOutSelf);

    Inode_Relinquish(rootDir);
    return err;
}



errno_t Process_Create(pid_t pid, pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull pFileHierarchy, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask, ProcessRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessRef self;
    
    try(Object_Create(class(Process), 0, (void**)&self));

    mtx_init(&self->mtx);

    self->state = PS_ALIVE;
    self->pid = pid;
    self->ppid = ppid;
    self->pgrp = pgrp;
    self->sid = sid;
    self->catalogId = kCatalogId_None;

    List_Init(&self->vpQueue);
    self->next_avail_vcpuid = VCPUID_MAIN + 1;

    try(IOChannelTable_Init(&self->ioChannelTable));

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        List_Init(&self->waitQueueTable[i]);
    }
    self->nextAvailWaitQueueId = 0;

    wq_init(&self->sleepQueue);
    wq_init(&self->siwaQueue);
    FileManager_Init(&self->fm, pFileHierarchy, uid, gid, pRootDir, pWorkingDir, umask);

    try(AddressSpace_Create(&self->addressSpace));

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

void Process_deinit(ProcessRef _Nonnull self)
{
    assert(List_IsEmpty(&self->children));
    
    IOChannelTable_Deinit(&self->ioChannelTable);
    FileManager_Deinit(&self->fm);
    AddressSpace_Destroy(self->addressSpace);

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        List_ForEach(&self->waitQueueTable[i], ListNode, {
            UWaitQueue* cwp = (UWaitQueue*)pCurNode;

            uwq_destroy(cwp);
        });
    }

    wq_deinit(&self->siwaQueue);
    wq_deinit(&self->sleepQueue);

    List_Deinit(&self->vpQueue);
    
    self->addressSpace = NULL;
    self->imageBase = NULL;
    self->argumentsBase = NULL;

    self->pid = 0;
    self->ppid = 0;

    mtx_deinit(&self->mtx);
}

errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp)
{
    decl_try_err();
    vcpu_t vp = NULL;
    VirtualProcessorParameters kp;

    *idp = 0;

    mtx_lock(&self->mtx);
    const bool isMainVcpu = self->vpQueue.first == NULL;

    kp.func = (VoidFunc_1)attr->func;
    kp.context = attr->arg;
    kp.ret_func = (self->vpQueue.first) ? vcpu_uret_relinquish_self : vcpu_uret_exit;
    kp.kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    kp.userStackSize = __max(attr->stack_size, VP_DEFAULT_USER_STACK_SIZE);
    if (isMainVcpu) {
        kp.id = VCPUID_MAIN;
        kp.groupid = VCPUID_MAIN_GROUP;
    }
    else {
        kp.id = self->next_avail_vcpuid++;
        kp.groupid = attr->groupid;
    }
    kp.priority = attr->priority;
    kp.isUser = true;

    if (self->state >= PS_ZOMBIFYING) {
        throw(EINTR);
    }

    try(vcpu_pool_acquire(
            g_vcpu_pool,
            &kp,
            &vp));

    vp->proc = self;
    vp->udata = attr->data;
    List_InsertAfterLast(&self->vpQueue, &vp->owner_qe);
    *idp = vp->id;

    if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
        vcpu_resume(vp, false);
    }

catch:
    mtx_unlock(&self->mtx);

    return err;
}

void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp)
{
    Process_DetachVirtualProcessor(self, vp);
    vcpu_pool_relinquish(g_vcpu_pool, vcpu_current());
    /* NOT REACHED */
}

void Process_DetachVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp)
{
    assert(vp->proc == self);

    mtx_lock(&self->mtx);
    List_Remove(&self->vpQueue, &vp->owner_qe);
    vp->proc = NULL;
    mtx_unlock(&self->mtx);
}

errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo)
{
    decl_try_err();
    bool hasMatched = false;

    if (scope == SIG_SCOPE_VCPU && id == 0) {
        return vcpu_sigsend(vcpu_current(), signo, false);
    }

    mtx_lock(&self->mtx);
    List_ForEach(&self->vpQueue, ListNode, {
        vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);
        int doSend;
        bool isProc;

        switch (scope) {
            case SIG_SCOPE_VCPU:
                doSend = (id == cvp->id);
                isProc = false;
                break;

            case SIG_SCOPE_VCPU_GROUP:
                doSend = (id == cvp->groupid);
                isProc = false;
                break;

            case SIG_SCOPE_PROC:
                doSend = 1;
                isProc = true;
                break;

            default:
                abort();
        }

        if (doSend) {
            err = vcpu_sigsend(cvp, signo, isProc);
            if (err != EOK) {
                break;
            }
            hasMatched = true;
        }
    });
    mtx_unlock(&self->mtx);

    if (!hasMatched && err == EOK) {
        err = ESRCH;
    }

    return err;
}

excpt_handler_t Process_SetExceptionHandler(ProcessRef _Nonnull self, excpt_handler_t _Nullable handler, void* _Nullable arg)
{
    excpt_handler_t oh = self->excpt_handler;

    self->excpt_handler = handler;
    self->excpt_arg = arg;
    return oh;
}

excpt_handler_t _Nonnull Process_Exception(ProcessRef _Nonnull self, const excpt_info_t* _Nonnull ei, excpt_ctx_t* _Nonnull ec)
{
    vcpu_t vp = vcpu_current();
    excpt_handler_t handler = vp->excpt_handler;
    void* arg = vp->excpt_arg;

    if (handler == NULL) {
        handler = self->excpt_handler;
        arg = self->excpt_arg;
    }

    if (handler == NULL) {
        Process_Exit(self, JREASON_EXCEPTION, ei->code);
        /* NOT REACHED */
    }


    uintptr_t usp = usp_get();
    usp = sp_push_bytes(usp, ei, sizeof(excpt_info_t));
    uintptr_t ei_usp = usp;
    usp = sp_push_bytes(usp, ec, sizeof(excpt_ctx_t));
    uintptr_t ec_usp = usp;

    usp = sp_push_ptr(usp, (void*)ec_usp);
    usp = sp_push_ptr(usp, (void*)ei_usp);
    usp = sp_push_ptr(usp, arg);

    usp = sp_push_rts(usp, (void*)0);
    usp_set(usp);

    return handler;
}
