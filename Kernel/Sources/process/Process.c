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

    try(IOChannelTable_Init(&self->ioChannelTable));

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        List_Init(&self->waitQueueTable[i]);
    }
    self->nextAvailWaitQueueId = 0;

    wq_init(&self->sleepQueue);
    wq_init(&self->siwaQueue);
    FileManager_Init(&self->fm, pFileHierarchy, uid, gid, pRootDir, pWorkingDir, umask);

    cnd_init(&self->procTermSignaler);

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

    cnd_deinit(&self->procTermSignaler);

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

    kp.func = (VoidFunc_1)attr->func;
    kp.context = attr->arg;
    kp.ret_func = (self->vpQueue.first) ? vcpu_uret_relinquish_self : vcpu_uret_exit;
    kp.kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    kp.userStackSize = __max(attr->stack_size, VP_DEFAULT_USER_STACK_SIZE);
    kp.vpgid = attr->groupid;
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
    *idp = vp->vpid;

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

static errno_t _sigsend(vcpu_t _Nonnull vp, int signo)
{
    const int sps = preempt_disable();
    const errno_t err = vcpu_sendsignal(vp, signo);
    preempt_restore(sps);
    
    return err;
}

errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo)
{
    decl_try_err();
    bool hasMatched = false;

    if (scope == SIG_SCOPE_VCPU && id == 0) {
        return _sigsend(vcpu_current(), signo);
    }

    mtx_lock(&self->mtx);
    List_ForEach(&self->vpQueue, ListNode, {
        vcpu_t cvp = VP_FROM_OWNER_NODE(pCurNode);
        int doSend;

        switch (scope) {
            case SIG_SCOPE_VCPU:
                doSend = (id == cvp->vpid);
                break;

            case SIG_SCOPE_VCPU_GROUP:
                doSend = (id == cvp->vpgid);
                break;

            case SIG_SCOPE_PROC:
                doSend = 1;
                break;

            default:
                abort();
        }

        if (doSend) {
            err = _sigsend(cvp, signo);
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
