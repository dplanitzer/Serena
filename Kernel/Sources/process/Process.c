//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/FilesystemManager.h>


CLASS_METHODS(Process, Object,
OVERRIDE_METHOD_IMPL(deinit, Process, Object)
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
    FilesystemRef pRootFs = FilesystemManager_CopyRootFilesystem(gFilesystemManager);
    PathResolverRef pResolver = NULL;

    try(PathResolver_Create(pRootFs, &pResolver));
    try(Process_Create(1, kUser_Root, pResolver, FilePermissions_MakeFromOctal(0022), pOutProc));

    return EOK;

catch:
    PathResolver_Destroy(pResolver);
    Object_Release(pRootFs);
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



errno_t Process_Create(int ppid, User user, PathResolverRef _Nonnull pResolver, FilePermissions fileCreationMask, ProcessRef _Nullable * _Nonnull pOutProc)
{
    decl_try_err();
    ProcessRef pProc;
    DispatchQueueRef pMainDispatchQueue = NULL;
    int mainDispatchQueueDesc = -1;
    
    try(Object_Create(Process, &pProc));

    Lock_Init(&pProc->lock);

    pProc->ppid = ppid;
    pProc->pid = Process_GetNextAvailablePID();

    try(ObjectArray_Init(&pProc->ioChannels, INITIAL_IOCHANNELS_CAPACITY));
    try(ObjectArray_Init(&pProc->privateResources, INITIAL_PRIVATE_RESOURCES_CAPACITY));
    try(IntArray_Init(&pProc->childPids, 0));

    pProc->pathResolver = pResolver;
    pProc->fileCreationMask = fileCreationMask;
    pProc->realUser = user;

    List_Init(&pProc->tombstones);
    ConditionVariable_Init(&pProc->tombstoneSignaler);

    try(DispatchQueue_Create(0, 1, kDispatchQoS_Interactive, kDispatchPriority_Normal, gVirtualProcessorPool, pProc, &pMainDispatchQueue));
    try(Process_RegisterPrivateResource_Locked(pProc, (ObjectRef) pMainDispatchQueue, &mainDispatchQueueDesc));
    Object_Release(pMainDispatchQueue);
    pProc->mainDispatchQueue = pMainDispatchQueue;
    pMainDispatchQueue = NULL;
    assert(mainDispatchQueueDesc == 0);

    try(AddressSpace_Create(&pProc->addressSpace));

    *pOutProc = pProc;
    return EOK;

catch:
    Object_Release(pMainDispatchQueue);
    Object_Release(pProc);
    *pOutProc = NULL;
    return err;
}

void Process_deinit(ProcessRef _Nonnull pProc)
{
    Process_CloseAllIOChannels_Locked(pProc);
    ObjectArray_Deinit(&pProc->ioChannels);

    Process_DisposeAllPrivateResources_Locked(pProc);
    ObjectArray_Deinit(&pProc->privateResources);

    PathResolver_Destroy(pProc->pathResolver);

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

// Destroys the private resource identified by the given descriptor. The resource
// is deallocated and removed from the resource table.
errno_t Process_DisposePrivateResource(ProcessRef _Nonnull pProc, int od)
{
    decl_try_err();
    ObjectRef pResource;

    if ((err = Process_UnregisterPrivateResource(pProc, od, &pResource)) == EOK) {
        Object_Release(pResource);
    }
    return err;
}

// Allocates more (user) address space to the given process.
errno_t Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ssize_t count, void* _Nullable * _Nonnull pOutMem)
{
    return AddressSpace_Allocate(pProc->addressSpace, count, pOutMem);
}
