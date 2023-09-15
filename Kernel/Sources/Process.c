//
//  Process.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "GemDosExecutableLoader.h"


ProcessRef  gRootProcess;


// Returns the next PID available for use by a new process.
Int Process_GetNextAvailablePID(void)
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



ErrorCode Process_Create(Int pid, ProcessRef _Nullable * _Nonnull pOutProc)
{
    decl_try_err();
    Process* pProc;
    
    try(kalloc_cleared(sizeof(Process), (Byte**) &pProc));
    
    pProc->pid = pid;
    Lock_Init(&pProc->lock);

    try(DispatchQueue_Create(0, 1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRIORITY_NORMAL, gVirtualProcessorPool, pProc, &pProc->mainDispatchQueue));
    try(AddressSpace_Create(&pProc->addressSpace));

    List_Init(&pProc->children);
    ListNode_Init(&pProc->siblings);

    *pOutProc = pProc;
    return EOK;

catch:
    Process_Destroy(pProc);
    *pOutProc = NULL;
    return err;
}

void Process_Destroy(ProcessRef _Nullable pProc)
{
    if (pProc) {
        ListNode_Deinit(&pProc->siblings);
        List_Deinit(&pProc->children);
        pProc->parent = NULL;

        AddressSpace_Destroy(pProc->addressSpace);
        pProc->addressSpace = NULL;
        pProc->imageBase = NULL;
        pProc->argumentsBase = NULL;

        DispatchQueue_Destroy(pProc->mainDispatchQueue);
        pProc->mainDispatchQueue = NULL;

        Lock_Deinit(&pProc->lock);
        pProc->pid = 0;

        kfree((Byte*) pProc);
    }
}

// Stops the given process and all children, grand-children, etc.
static void Process_RecursivelyStop(ProcessRef _Nonnull pProc)
{
    // Be sure to mark the process as terminating
    AtomicBool_Set(&pProc->isTerminating, true);


    // Terminate all dispatch queues. This takes care of aborting user space
    // invocations.
    DispatchQueue_Terminate(pProc->mainDispatchQueue);


    // Wait for all dispatch queues to have reached 'terminated' state
    DispatchQueue_WaitForTerminationCompleted(pProc->mainDispatchQueue);


    // Recursively stop all children
    Lock_Lock(&pProc->lock);
    ProcessRef pCurChild = (ProcessRef) pProc->children.first;
    while(pCurChild) {
        Process_RecursivelyStop(pCurChild);
        pCurChild = (ProcessRef) pCurChild->siblings.next;
    }
    Lock_Unlock(&pProc->lock);
}

// Recursively destroys a tree of processes. The destruction is executed bottom-
// up.
static void Process_RecursivelyDestroy(ProcessRef _Nonnull pProc)
{
    Bool done = false;

    // Recurse down the process tree
    while (!done) {
        ProcessRef pChildProc;

        Lock_Lock(&pProc->lock);
        if (!List_IsEmpty(&pProc->children)) {
            pChildProc = (ProcessRef) List_RemoveFirst(&pProc->children);
        } else {
            pChildProc = NULL;
        }
        Lock_Unlock(&pProc->lock);

        if (pChildProc) {
            Process_RecursivelyDestroy(pChildProc);
        } else {
            done = true;
        }
    }


    // Destroy processes on the way back up the tree
    Process_Destroy(pProc);
}

// Runs on the kernel main dispatch queue and terminates the given process.
static void _Process_DoTerminate(ProcessRef _Nonnull pProc)
{
    // Notes on terminating a process:
    //
    // All VPs belonging to a process are executing call-as-user invocations. The
    // first step of terminating a process is to abort all these invocations. This
    // is done by terminating all dispatch queues that belong to the process first.
    //
    // What does aborting a call-as-user invocation mean?
    // 1) If a VP is currently executing in user space then the user space
    //    invocation is aborted and the VP returns back to the dispatch queue
    //    main loop.
    // 2) If a VP is currently executing inside a system call then this system
    //    call has to first complete and we then abort the user space invocation
    //    that led to the system call when the system call would normally return
    //    to user space. So the return to user space is redirected to a piece of
    //    code that aborts the user space invocation. The VP then returns back
    //    to the dispatch queue main loop.
    // 3) A VP may be in waiting state because it was executing a system call that
    //    invoked a blocking function. This wait will be interrupted/aborted as
    //    a side-effect of aborting the call-as-user invocation. Additionally all
    //    further abortable waits that the VP wants to take are immediately aborted
    //    until the VP has left the system call. This auto-abort does not apply
    //    to non-abortable waits like Lock_Lock.
    // 
    // Terminating a dispatch queue means that all queued up work items and timers
    // are flushed from the queue and that the queue relinquishes all its VPs. The
    // queue also stops accepting new work.
    //
    // A word on process termination and system calls:
    //
    // A system call MUST complete its run before the process data structures can
    // be freed. This is required because a system call manipulates kernel state
    // and we must ensure that every state manipulation is properly finalized
    // before we continue.
    // Note also that a system call that takes a kernel lock must eventually drop
    // this lock (it can not endlessly hold it) and it is expected to drop the
    // lock ASAP (it can not take unecessarily long to release the lock). That's
    // why it is fine that Lock_Lock() is not interruptable even in the face of
    // the ability to terminate a process voluntarily/involuntarily.
    // The top-level system call handler checks whether a process is terminating
    // and it aborts the user space invocation that led to the system call. This
    // is the only required process termination check in a system call. All other
    // checks are voluntarily.
    // That said, every wait also does a check for process termination and the
    // wait immediatly returns with an EINTR if the process is in the process of
    // being terminated. The only exception to this is the wait that Lock_Lock()
    // does since this kind of lock is a kernel lock that is used to preserve the
    // integrity of kernel data structures.

    // Notes on terminating a process tree:
    //
    // If a process terminates voluntarily or involuntarily will by default
    // also terminate all its child, grand-child, etc processes. This is done in
    // a way where the individual steps of the termination are not observable to
    // the child nor the parent process. This means that termination is atomic
    // from the viewpoint of the parent and the child.
    //
    // The way we achieve this is by breaking the termination down into two
    // separate phases: the first one goes top-down and puts a process and all
    // its children at rest by terminating their dispatch queues and the second
    // phase then goes bottom-up and frees the process and all its children.


    // Stop this process and all its children before we free any resources
    Process_RecursivelyStop(pProc);


    // Finally destroy the process and all its children.
    Process_RecursivelyDestroy(pProc);
}

// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
void Process_Terminate(ProcessRef _Nonnull pProc, Int exitCode)
{
    // We do not allow exiting the root process
    if (pProc == gRootProcess) {
        abort();
    }


    // Mark the process atomically as terminating. Leave now if some other VP
    // belonging to this process has already kicked off the termination. Note
    // that if multiple VPs concurrently execute a Process_Terminate(), that
    // at most one of them is able to get past this gate to kick off the
    // termination. All other VPs will return and their system calls will be
    // aborted. Also note that the Process data structure stays alive until
    // after _all_ VPs (including the first one) have returned from their
    // (aborted) system calls. So by the time the process data structure is
    // freed no system call that might directly or indirectly reference the
    // process is active anymore because all of them have been aborted and
    // unwound before we free the process data structure.
    if(AtomicBool_Set(&pProc->isTerminating, true)) {
        return;
    }


    // Remember the exit code
    pProc->exitCode = exitCode;


    // Schedule the actual process termination and destruction on the kernel
    // main dispatch queue.
    try_bang(DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func) _Process_DoTerminate, (Byte*) pProc)));
}

Bool Process_IsTerminating(ProcessRef _Nonnull pProc)
{
    return pProc->isTerminating;
}

Int Process_GetPid(ProcessRef _Nonnull pProc)
{
    // The PID is constant over the lifetime of the process. No need to lock here
    return pProc->pid;
}

Int Process_GetParentPid(ProcessRef _Nonnull pProc)
{
    // Need to lock to protect against the process destruction since we're accessing the parent field
    Lock_Lock(&pProc->lock);
    assert(pProc->parent);
    const Int ppid = pProc->parent->pid;
    Lock_Unlock(&pProc->lock);

    return ppid;
}

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
void* Process_GetArgumentsBaseAddress(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    void* ptr = pProc->argumentsBase;
    Lock_Unlock(&pProc->lock);
    return ptr;
}

static ByteCount calc_size_of_arg_table(const Character* const _Nullable * _Nullable pTable, Int* _Nonnull pOutTableEntryCount)
{
    ByteCount nbytes = 0;
    Int count = 0;

    if (pTable != NULL) {
        while (*pTable != NULL) {
            nbytes += String_Length(*pTable);
            pTable++;
        }
        count++;
    }
    *pOutTableEntryCount = count;

    return nbytes;
}

static ErrorCode Process_CopyInProcessArguments(ProcessRef _Nonnull pProc, const Character* const _Nullable * _Nullable pArgv, const Character* const _Nullable * _Nullable pEnv)
{
    decl_try_err();
    Int nArgvCount = 0;
    Int nEnvCount = 0;
    const ByteCount nbytes_argv = calc_size_of_arg_table(pArgv, &nArgvCount);
    const ByteCount nbytes_envp = calc_size_of_arg_table(pEnv, &nEnvCount);
    const ByteCount nbytes_procargs = __Ceil_PowerOf2(sizeof(ProcessArguments) + nbytes_argv + nbytes_envp, CPU_PAGE_SIZE);

    try(AddressSpace_Allocate(pProc->addressSpace, nbytes_procargs, &pProc->argumentsBase));

    ProcessArguments* pProcArgs = (ProcessArguments*) pProc->argumentsBase;
    Character** pProcArgv = (Character**)(pProc->argumentsBase + sizeof(ProcessArguments));
    Character** pProcEnv = (Character**)&pProcArgv[nArgvCount + 1];
    Character*  pDst = (Character*)&pProcEnv[nEnvCount + 1];
    const Character** pSrcArgv = (const Character**) pArgv;
    const Character** pSrcEnv = (const Character**) pEnv;


    // Argv
    for (Int i = 0; i < nArgvCount; i++) {
        const Character* pSrc = (const Character*)pSrcArgv[i];

        pProcArgv[i] = pDst;
        pDst = String_Copy(pDst, pSrc);
    }
    pProcArgv[nArgvCount] = NULL;


    for (Int i = 0; i < nEnvCount; i++) {
        const Character* pSrc = (const Character*)pSrcEnv[i];

        pProcEnv[i] = pDst;
        pDst = String_Copy(pDst, pSrc);
    }
    pProcEnv[nEnvCount] = NULL;


    // Descriptor
    pProcArgs->version = sizeof(ProcessArguments);
    pProcArgs->reserved = 0;
    pProcArgs->arguments_size = nbytes_procargs;
    pProcArgs->argc = nArgvCount;
    pProcArgs->argv = pProcArgv;
    pProcArgs->envp = pProcEnv;
    pProcArgs->image_base = NULL;

    return EOK;

catch:
    return err;
}

// Loads an executable from the given executable file into the process address
// space.
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be loacted at the address 'pExecAddr'
ErrorCode Process_Exec(ProcessRef _Nonnull pProc, Byte* _Nonnull pExecAddr, const Character* const _Nullable * _Nullable pArgv, const Character* const _Nullable * _Nullable pEnv)
{
    GemDosExecutableLoader loader;
    Byte* pEntryPoint = NULL;
    decl_try_err();
    
    Lock_Lock(&pProc->lock);

    // XXX for now to keep loading simpler
    assert(pProc->imageBase == NULL);

    // Copy the process arguments into the process address space
    try(Process_CopyInProcessArguments(pProc, pArgv, pEnv));

    // Load the executable
    GemDosExecutableLoader_Init(&loader, pProc->addressSpace);
    try(GemDosExecutableLoader_Load(&loader, pExecAddr, &pProc->imageBase, &pEntryPoint));
    GemDosExecutableLoader_Deinit(&loader);

    ((ProcessArguments*) pProc->argumentsBase)->image_base = pProc->imageBase;

    try(DispatchQueue_DispatchAsync(pProc->mainDispatchQueue, DispatchQueueClosure_MakeUser((Closure1Arg_Func)pEntryPoint, pProc->argumentsBase)));

    Lock_Unlock(&pProc->lock);

    return EOK;

catch:
    Lock_Unlock(&pProc->lock);
    return err;
}

ErrorCode Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure)
{
    return DispatchQueue_DispatchAsync(pProc->mainDispatchQueue, DispatchQueueClosure_MakeUser(pUserClosure, NULL));
}

// Allocates more (user) address space to the given process.
ErrorCode Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ByteCount count, Byte* _Nullable * _Nonnull pOutMem)
{
    return AddressSpace_Allocate(pProc->addressSpace, count, pOutMem);
}

// Adds the given process as a child to the given process. 'pOtherProc' must not
// already be a child of another process.
ErrorCode Process_AddChildProcess(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc)
{
    if (pOtherProc->parent) {
        return EPARAM;
    }

    Lock_Lock(&pProc->lock);
    List_InsertAfterLast(&pProc->children, &pOtherProc->siblings);
    pOtherProc->parent = pProc;
    Lock_Unlock(&pProc->lock);

    return EOK;
}

// Removes the given process from 'pProc'. Does nothing if the given process is
// not a child of 'pProc'.
void Process_RemoveChildProcess(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc)
{
    Lock_Lock(&pProc->lock);
    if (pOtherProc->parent == pProc) {
        List_Remove(&pProc->children, &pOtherProc->siblings);
        pOtherProc->parent = NULL;
    }
    Lock_Unlock(&pProc->lock);
}
