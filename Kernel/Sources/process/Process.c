//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "UDispatchQueue.h"
#include <filemanager/FileHierarchy.h>


class_func_defs(Process, Object,
override_func_def(deinit, Process, Object)
);


// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
ProcessRef _Nullable Process_GetCurrent(void)
{
    DispatchQueueRef pQueue = DispatchQueue_GetCurrent();

    return (pQueue != NULL) ? DispatchQueue_GetOwningProcess(pQueue) : NULL;
}


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
    UDispatchQueueRef pMainQueue = NULL;
    int mainQueueDesc = -1;
    
    try(Object_Create(class(Process), 0, (void**)&self));

    Lock_Init(&self->lock);

    self->pid = pid;
    self->ppid = ppid;
    self->pgrp = pgrp;
    self->sid = sid;
    self->catalogId = kCatalogId_None;

    List_Init(&self->vpQueue);

    try(IOChannelTable_Init(&self->ioChannelTable));
    try(UResourceTable_Init(&self->uResourcesTable));

    WaitQueue_Init(&self->sleepQueue);
    WaitQueue_Init(&self->siwaQueue);
    FileManager_Init(&self->fm, pFileHierarchy, uid, gid, pRootDir, pWorkingDir, umask);

    List_Init(&self->tombstones);
    ConditionVariable_Init(&self->tombstoneSignaler);

    try(UDispatchQueue_Create(0, 1, kDispatchQoS_Interactive, kDispatchPriority_Normal, gVirtualProcessorPool, self, &pMainQueue));
    self->mainDispatchQueue = pMainQueue->dispatchQueue;
    try(UResourceTable_AdoptResource(&self->uResourcesTable, (UResourceRef) pMainQueue, &mainQueueDesc));
    DispatchQueue_SetDescriptor(self->mainDispatchQueue, mainQueueDesc);
    assert(mainQueueDesc == 0);

    try(AddressSpace_Create(&self->addressSpace));

    *pOutSelf = self;
    return EOK;

catch:
    UResource_Dispose(pMainQueue);
    self->mainDispatchQueue = NULL;
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

void Process_deinit(ProcessRef _Nonnull self)
{
    assert(List_IsEmpty(&self->children));
    
    IOChannelTable_Deinit(&self->ioChannelTable);
    UResourceTable_Deinit(&self->uResourcesTable);

    FileManager_Deinit(&self->fm);

    Object_Release(self->terminationNotificationQueue);
    self->terminationNotificationQueue = NULL;
    self->terminationNotificationClosure = NULL;
    self->terminationNotificationContext = NULL;
    
    Process_DestroyAllTombstones_Locked(self);
    ConditionVariable_Deinit(&self->tombstoneSignaler);

    AddressSpace_Destroy(self->addressSpace);

    WaitQueue_Deinit(&self->siwaQueue);
    WaitQueue_Deinit(&self->sleepQueue);

    List_Deinit(&self->vpQueue);
    
    self->addressSpace = NULL;
    self->imageBase = NULL;
    self->argumentsBase = NULL;
    self->mainDispatchQueue = NULL;

    self->pid = 0;
    self->ppid = 0;

    Lock_Deinit(&self->lock);
}

errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp)
{
    decl_try_err();
    VirtualProcessor* vp = NULL;
    VirtualProcessorParameters kp;

    *idp = 0;

    kp.func = (VoidFunc_1)attr->func;
    kp.context = attr->arg;
    kp.ret_func = cpu_relinquish_from_user;
    kp.kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    kp.userStackSize = __max(attr->stack_size, VP_DEFAULT_USER_STACK_SIZE);
    kp.vpgid = attr->groupid;
    kp.priority = attr->priority;
    kp.isUser = true;

    Lock_Lock(&self->lock);

    if (!self->isTerminating) {
        err = VirtualProcessorPool_AcquireVirtualProcessor(
                                            gVirtualProcessorPool,
                                            &kp,
                                            &vp);
        if (err == EOK) {
            vp->proc = self;
            vp->udata = attr->data;
            List_InsertAfterLast(&self->vpQueue, &vp->owner_qe);
            *idp = vp->vpid;

            if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
                VirtualProcessor_Resume(vp, false);
            }
        }
    }
    else {
        err = ESRCH;
    }

    Lock_Unlock(&self->lock);

    return err;
}
void _vcpu_relinquish_self(void)
{
    VirtualProcessor* vp = VirtualProcessor_GetCurrent();

    Process_RelinquishVirtualProcessor(vp->proc, vp);
    /* NOT REACHED */
}

void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, VirtualProcessor* _Nonnull vp)
{
    Lock_Lock(&self->lock);
    assert(vp->proc == self);
    List_Remove(&self->vpQueue, &vp->owner_qe);
    vp->proc = NULL;
    Lock_Unlock(&self->lock);

    VirtualProcessorPool_RelinquishVirtualProcessor(gVirtualProcessorPool, VirtualProcessor_GetCurrent());
    /* NOT REACHED */
}
