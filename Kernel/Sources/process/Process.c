//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "UDispatchQueue.h"
#include <filesystem/FilesystemManager.h>


class_func_defs(Process, Object,
override_func_def(deinit, Process, Object)
);


// Returns the next PID available for use by a new process.
static ProcessId Process_GetNextAvailablePID(void)
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


errno_t RootProcess_Create(ProcessRef _Nullable * _Nonnull pOutProc)
{
    decl_try_err();
    FilesystemRef pFS = FilesystemManager_CopyRootFilesystem(gFilesystemManager);
    InodeRef pRootDir = NULL;

    assert(pFS != NULL);
    try(Filesystem_AcquireRootNode(pFS, &pRootDir));
    try(Process_Create(1, kUser_Root, pRootDir, pRootDir, FilePermissions_MakeFromOctal(0022), pOutProc));
    Inode_Relinquish(pRootDir);
    Object_Release(pFS);

    return EOK;

catch:
    Inode_Relinquish(pRootDir);
    Object_Release(pFS);
    *pOutProc = NULL;
    return err;
}

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param pProc the process into which the executable image should be loaded
// \param pExecPath path to a GemDOS executable file
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
errno_t RootProcess_Exec(ProcessRef _Nonnull pProc, const char* _Nonnull pExecPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = Process_Exec_Locked(pProc, pExecPath, NULL, NULL);
    Lock_Unlock(&pProc->lock);
    return err;
}



errno_t Process_Create(int ppid, User user, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, FilePermissions fileCreationMask, ProcessRef _Nullable * _Nonnull pOutProc)
{
    decl_try_err();
    ProcessRef pProc;
    UDispatchQueueRef pMainQueue = NULL;
    int mainQueueDesc = -1;
    
    try(Object_Create(Process, &pProc));

    Lock_Init(&pProc->lock);

    pProc->ppid = ppid;
    pProc->pid = Process_GetNextAvailablePID();

    try(IOChannelTable_Init(&pProc->ioChannelTable));
    try(UResourceTable_Init(&pProc->uResourcesTable));
    try(IntArray_Init(&pProc->childPids, 0));

    pProc->rootDirectory = Inode_AcquireUnlocked(pRootDir);
    pProc->workingDirectory = Inode_AcquireUnlocked(pWorkingDir);
    pProc->fileCreationMask = fileCreationMask;
    pProc->realUser = user;

    List_Init(&pProc->tombstones);
    ConditionVariable_Init(&pProc->tombstoneSignaler);

    try(UDispatchQueue_Create(0, 1, kDispatchQoS_Interactive, kDispatchPriority_Normal, gVirtualProcessorPool, pProc, &pMainQueue));
    pProc->mainDispatchQueue = pMainQueue->dispatchQueue;
    try(UResourceTable_AdoptResource(&pProc->uResourcesTable, (UResourceRef) pMainQueue, &mainQueueDesc));
    DispatchQueue_SetDescriptor(pProc->mainDispatchQueue, mainQueueDesc);
    assert(mainQueueDesc == 0);

    try(AddressSpace_Create(&pProc->addressSpace));

    *pOutProc = pProc;
    return EOK;

catch:
    UResource_Dispose(pMainQueue);
    pProc->mainDispatchQueue = NULL;
    Object_Release(pProc);
    *pOutProc = NULL;
    return err;
}

void Process_deinit(ProcessRef _Nonnull pProc)
{
    IOChannelTable_Deinit(&pProc->ioChannelTable);
    UResourceTable_Deinit(&pProc->uResourcesTable);

    Inode_Relinquish(pProc->workingDirectory);
    pProc->workingDirectory = NULL;
    Inode_Relinquish(pProc->rootDirectory);
    pProc->rootDirectory = NULL;

    Process_DestroyAllTombstones_Locked(pProc);
    ConditionVariable_Deinit(&pProc->tombstoneSignaler);
    IntArray_Deinit(&pProc->childPids);

    AddressSpace_Destroy(pProc->addressSpace);
    pProc->addressSpace = NULL;
    pProc->imageBase = NULL;
    pProc->argumentsBase = NULL;
    pProc->mainDispatchQueue = NULL;

    pProc->pid = 0;
    pProc->ppid = 0;

    Lock_Deinit(&pProc->lock);
}

ProcessId Process_GetId(ProcessRef _Nonnull pProc)
{
    // The PID is constant over the lifetime of the process. No need to lock here
    return pProc->pid;
}

ProcessId Process_GetParentId(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const ProcessId ppid = pProc->ppid;
    Lock_Unlock(&pProc->lock);

    return ppid;
}

UserId Process_GetRealUserId(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const UserId uid = pProc->realUser.uid;
    Lock_Unlock(&pProc->lock);

    return uid;
}

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
void* _Nonnull Process_GetArgumentsBaseAddress(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    void* ptr = pProc->argumentsBase;
    Lock_Unlock(&pProc->lock);
    return ptr;
}

// Allocates more (user) address space to the given process.
errno_t Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ssize_t count, void* _Nullable * _Nonnull pOutMem)
{
    return AddressSpace_Allocate(pProc->addressSpace, count, pOutMem);
}

void Process_MakePathResolver(ProcessRef _Nonnull self, PathResolverRef _Nonnull pResolver)
{
    PathResolver_Init(pResolver, self->rootDirectory, self->workingDirectory, self->realUser);
}
