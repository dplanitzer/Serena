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


// Returns the next PID available for use by a new process.
static pid_t Process_GetNextAvailablePID(void)
{
    static volatile AtomicInt gNextAvailablePid = 0;
    return AtomicInt_Increment(&gNextAvailablePid);
}

// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
ProcessRef _Nullable Process_GetCurrent(void)
{
    DispatchQueueRef pQueue = DispatchQueue_GetCurrent();

    return (pQueue != NULL) ? DispatchQueue_GetOwningProcess(pQueue) : NULL;
}


errno_t RootProcess_Create(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutProc)
{
    InodeRef rootDir = FileHierarchy_AcquireRootDirectory(pRootFh);
    const errno_t err = Process_Create(1, pRootFh, kUserId_Root, kGroupId_Root, rootDir, rootDir, perm_from_octal(0022), pOutProc);

    Inode_Relinquish(rootDir);
    return err;
}



errno_t Process_Create(int ppid, FileHierarchyRef _Nonnull pFileHierarchy, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask, ProcessRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessRef self;
    UDispatchQueueRef pMainQueue = NULL;
    int mainQueueDesc = -1;
    
    try(Object_Create(class(Process), 0, (void**)&self));

    Lock_Init(&self->lock);

    self->ppid = ppid;
    self->pid = Process_GetNextAvailablePID();
    self->catalogId = kCatalogId_None;

    try(IOChannelTable_Init(&self->ioChannelTable));
    try(UResourceTable_Init(&self->uResourcesTable));

    WaitQueue_Init(&self->sleepQueue);
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

    WaitQueue_Deinit(&self->sleepQueue);

    self->addressSpace = NULL;
    self->imageBase = NULL;
    self->argumentsBase = NULL;
    self->mainDispatchQueue = NULL;

    self->pid = 0;
    self->ppid = 0;

    Lock_Deinit(&self->lock);
}

pid_t Process_GetId(ProcessRef _Nonnull self)
{
    // The PID is constant over the lifetime of the process. No need to lock here
    return self->pid;
}

pid_t Process_GetParentId(ProcessRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const pid_t ppid = self->ppid;
    Lock_Unlock(&self->lock);

    return ppid;
}

uid_t Process_GetRealUserId(ProcessRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const uid_t uid = FileManager_GetRealUserId(&self->fm);
    Lock_Unlock(&self->lock);

    return uid;
}

gid_t Process_GetRealGroupId(ProcessRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const gid_t gid = FileManager_GetRealGroupId(&self->fm);
    Lock_Unlock(&self->lock);

    return gid;
}

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
void* _Nonnull Process_GetArgumentsBaseAddress(ProcessRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    void* ptr = self->argumentsBase;
    Lock_Unlock(&self->lock);
    return ptr;
}

// Allocates more (user) address space to the given process.
errno_t Process_AllocateAddressSpace(ProcessRef _Nonnull self, ssize_t count, void* _Nullable * _Nonnull pOutMem)
{
    return AddressSpace_Allocate(self->addressSpace, count, pOutMem);
}
