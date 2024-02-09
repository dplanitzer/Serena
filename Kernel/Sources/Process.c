//
//  Process.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "FilesystemManager.h"
#include "GemDosExecutableLoader.h"
#include "ProcessManager.h"
#include <krt/krt.h>


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
    User user = {kRootUserId, kRootGroupId};
    FilesystemRef pRootFileSys = FilesystemManager_CopyRootFilesystem(gFilesystemManager);
    InodeRef pRootDir = NULL;
    decl_try_err();

    try(Filesystem_AcquireRootNode(pRootFileSys, &pRootDir));
    try(Process_Create(1, user, pRootDir, pRootDir, FilePermissions_MakeFromOctal(0022), pOutProc));
    Filesystem_RelinquishNode(pRootFileSys, pRootDir);

    return EOK;

catch:
    Object_Release(pRootFileSys);
    *pOutProc = NULL;
    return err;
}

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param pProc the process into which the executable image should be loaded
// \param pExecAddr pointer to a GemDOS formatted executable file in memory
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be located at the address 'pExecAddr'
errno_t RootProcess_Exec(ProcessRef _Nonnull pProc, Byte* _Nonnull pExecAddr)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = Process_Exec_Locked(pProc, pExecAddr, NULL, NULL);
    Lock_Unlock(&pProc->lock);
    return err;
}



errno_t Process_Create(int ppid, User user, InodeRef _Nonnull  pRootDir, InodeRef _Nonnull pCurDir, FilePermissions fileCreationMask, ProcessRef _Nullable * _Nonnull pOutProc)
{
    decl_try_err();
    ProcessRef pProc;
    
    try(Object_Create(Process, &pProc));

    Lock_Init(&pProc->lock);

    pProc->ppid = ppid;
    pProc->pid = Process_GetNextAvailablePID();

    try(DispatchQueue_Create(0, 1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRIORITY_NORMAL, gVirtualProcessorPool, pProc, &pProc->mainDispatchQueue));
    try(AddressSpace_Create(&pProc->addressSpace));

    try(ObjectArray_Init(&pProc->ioChannels, INITIAL_DESC_TABLE_SIZE));
    try(IntArray_Init(&pProc->childPids, 0));

    try(PathResolver_Init(&pProc->pathResolver, pRootDir, pCurDir));
    pProc->fileCreationMask = fileCreationMask;
    pProc->realUser = user;

    List_Init(&pProc->tombstones);
    ConditionVariable_Init(&pProc->tombstoneSignaler);

    *pOutProc = pProc;
    return EOK;

catch:
    Object_Release(pProc);
    *pOutProc = NULL;
    return err;
}

void Process_deinit(ProcessRef _Nonnull pProc)
{
    Process_CloseAllIOChannels_Locked(pProc);
    ObjectArray_Deinit(&pProc->ioChannels);

    PathResolver_Deinit(&pProc->pathResolver);

    Process_DestroyAllTombstones_Locked(pProc);
    ConditionVariable_Deinit(&pProc->tombstoneSignaler);
    IntArray_Deinit(&pProc->childPids);

    AddressSpace_Destroy(pProc->addressSpace);
    pProc->addressSpace = NULL;
    pProc->imageBase = NULL;
    pProc->argumentsBase = NULL;

    DispatchQueue_Destroy(pProc->mainDispatchQueue);
    pProc->mainDispatchQueue = NULL;

    pProc->pid = 0;
    pProc->ppid = 0;

    Lock_Deinit(&pProc->lock);
}

// Frees all tombstones
void Process_DestroyAllTombstones_Locked(ProcessRef _Nonnull pProc)
{
    ProcessTombstone* pCurTombstone = (ProcessTombstone*)pProc->tombstones.first;

    while (pCurTombstone) {
        ProcessTombstone* nxt = (ProcessTombstone*)pCurTombstone->node.next;

        kfree(pCurTombstone);
        pCurTombstone = nxt;
    }
}

// Creates a new tombstone for the given child process with the given exit status
errno_t Process_OnChildDidTerminate(ProcessRef _Nonnull pProc, ProcessId childPid, int childExitCode)
{
    ProcessTombstone* pTombstone;

    if (Process_IsTerminating(pProc)) {
        // We're terminating ourselves. Let the child know so that it can bother
        // someone else (session leader) with it's tombstone request.
        return ESRCH;
    }

    if (kalloc_cleared(sizeof(ProcessTombstone), (void**) &pTombstone) != EOK) {
        print("Broken tombstone for %d:%d\n", pProc->pid, childPid);
        return EOK;
    }

    ListNode_Init(&pTombstone->node);
    pTombstone->pid = childPid;
    pTombstone->status = childExitCode;

    Lock_Lock(&pProc->lock);
    Process_AbandonChild_Locked(pProc, childPid);
    List_InsertAfterLast(&pProc->tombstones, &pTombstone->node);
    ConditionVariable_BroadcastAndUnlock(&pProc->tombstoneSignaler, &pProc->lock);
    
    return EOK;
}

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if there are no tombstones of terminated
// child processes available or the PID is not the PID of a child process of
// the receiver. Otherwise blocks the caller until the requested process or any
// child process (pid == -1) has exited.
errno_t Process_WaitForTerminationOfChild(ProcessRef _Nonnull pProc, ProcessId pid, ProcessTerminationStatus* _Nullable pStatus)
{
    decl_try_err();

    Lock_Lock(&pProc->lock);
    if (pid == -1 && List_IsEmpty(&pProc->tombstones)) {
        throw(ECHILD);
    }

    
    // Need to wait for a child to terminate
    while (true) {
        const ProcessTombstone* pTombstone = NULL;

        if (pid == -1) {
            // Any tombstone is good, return the first one (oldest) that was recorded
            pTombstone = (ProcessTombstone*)pProc->tombstones.first;
        } else {
            // Look for the specific child process
            List_ForEach(&pProc->tombstones, ProcessTombstone, {
                if (pCurNode->pid == pid) {
                    pTombstone = pCurNode;
                    break;
                }
            })

            if (pTombstone == NULL) {
                // Looks like the child isn't dead yet or 'pid' isn't referring to a child. Make sure it does
                if (!IntArray_Contains(&pProc->childPids, pid)) {
                    throw(ECHILD);
                }
            }
        }

        if (pTombstone) {
            if (pStatus) {
                pStatus->pid = pTombstone->pid;
                pStatus->status = pTombstone->status;
            }

            List_Remove(&pProc->tombstones, &pTombstone->node);
            kfree(pTombstone);
            break;
        }


        // Wait for a child to terminate
        try(ConditionVariable_Wait(&pProc->tombstoneSignaler, &pProc->lock, kTimeInterval_Infinity));
    }
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    Lock_Unlock(&pProc->lock);
    if (pStatus) {
        pStatus->pid = 0;
        pStatus->status = 0;
    }
    return err;
}

// Returns the PID of *any* of the receiver's children. This is used by the
// termination code to terminate all children. We don't care about the order
// in which we terminate the children but we do care that we trigger the
// termination of all of them. Keep in mind that a child may itself trigger its
// termination concurrently with our termination. The process is inherently
// racy and thus we need to be defensive about things. Returns 0 if there are
// no more children.
static int Process_GetAnyChildPid(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const int pid = IntArray_GetFirst(&pProc->childPids, -1);
    Lock_Unlock(&pProc->lock);
    return pid;
}

// Runs on the kernel main dispatch queue and terminates the given process.
void _Process_DoTerminate(ProcessRef _Nonnull pProc)
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
    // 3) A VP may be in waiting state because it executed a system call that
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
    // lock ASAP (it can not take unnecessarily long to release the lock). That's
    // why it is fine that Lock_Lock() is not interruptable even in the face of
    // the ability to terminate a process voluntarily/involuntarily.
    // The top-level system call handler checks whether a process is terminating
    // and it aborts the user space invocation that led to the system call. This
    // is the only required process termination check in a system call. All other
    // checks are voluntarily.
    // That said, every wait also does a check for process termination and the
    // wait immediately returns with an EINTR if the process is in the process of
    // being terminated. The only exception to this is the wait that Lock_Lock()
    // does since this kind of lock is a kernel lock that is used to preserve the
    // integrity of kernel data structures.

    // Notes on terminating a process tree:
    //
    // If a process terminates voluntarily or involuntarily then it'll by default
    // also terminate all its children, grand-children, etc processes. Every
    // process in the tree first terminates its children before it completes its
    // own termination. Doing it this way ensures that a parent process won't
    // (magically) disappear before all its children have terminated.


    // Terminate all dispatch queues. This takes care of aborting user space
    // invocations.
    DispatchQueue_Terminate(pProc->mainDispatchQueue);


    // Wait for all dispatch queues to have reached 'terminated' state
    DispatchQueue_WaitForTerminationCompleted(pProc->mainDispatchQueue);


    // Terminate all my children and wait for them to be dead
    while (true) {
        const ProcessId pid = Process_GetAnyChildPid(pProc);

        if (pid <= 0) {
            break;
        }

        ProcessRef pCurChild = ProcessManager_CopyProcessForPid(gProcessManager, pid);
        
        Process_Terminate(pCurChild, 0);
        Process_WaitForTerminationOfChild(pProc, pid, NULL);
        Object_Release(pCurChild);
    }


    // Let our parent know that we're dead now and that it should remember us by
    // commissioning a beautiful tombstone for us.
    if (!Process_IsRoot(pProc)) {
        ProcessRef pParentProc = ProcessManager_CopyProcessForPid(gProcessManager, pProc->ppid);

        if (pParentProc) {
            if (Process_OnChildDidTerminate(pParentProc, pProc->pid, pProc->exitCode) == ESRCH) {
                // XXXsession Try the session leader next. Give up if this fails too.
                // XXXsession. Unconditionally falling back to the root process for now.
                // Just drop the tombstone request if no one wants it.
                ProcessRef pRootProc = ProcessManager_CopyRootProcess(gProcessManager);
                Process_OnChildDidTerminate(pRootProc, pProc->pid, pProc->exitCode);
                Object_Release(pRootProc);
            }
            Object_Release(pParentProc);
        }
    }


    // Finally destroy the process.
    ProcessManager_Unregister(gProcessManager, pProc);
    Object_Release(pProc);
}

// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
void Process_Terminate(ProcessRef _Nonnull pProc, int exitCode)
{
    // We do not allow exiting the root process
    if (Process_IsRoot(pProc)) {
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
    try_bang(DispatchQueue_DispatchAsync(gMainDispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func) _Process_DoTerminate, pProc)));
}

bool Process_IsTerminating(ProcessRef _Nonnull pProc)
{
    return pProc->isTerminating;
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
void* Process_GetArgumentsBaseAddress(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    void* ptr = pProc->argumentsBase;
    Lock_Unlock(&pProc->lock);
    return ptr;
}

errno_t Process_SpawnChildProcess(ProcessRef _Nonnull pProc, const SpawnArguments* _Nonnull pArgs, ProcessId * _Nullable pOutChildPid)
{
    decl_try_err();
    ProcessRef pChildProc = NULL;
    bool needsUnlock = false;

    Lock_Lock(&pProc->lock);
    needsUnlock = true;

    const FilePermissions childUMask = ((pArgs->options & SPAWN_OVERRIDE_UMASK) != 0) ? (pArgs->umask & 0777) : pProc->fileCreationMask;
    try(Process_Create(pProc->pid, pProc->realUser, pProc->pathResolver.rootDirectory, pProc->pathResolver.currentWorkingDirectory, pProc->fileCreationMask, &pChildProc));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

    if ((pArgs->options & SPAWN_NO_DEFAULT_DESCRIPTOR_INHERITANCE) == 0) {
        const int nStdIoChannelsToInherit = __min(3, ObjectArray_GetCount(&pProc->ioChannels));

        for (int i = 0; i < nStdIoChannelsToInherit; i++) {
            IOChannelRef pCurChannel = (IOChannelRef) ObjectArray_GetAt(&pProc->ioChannels, i);

            if (pCurChannel) {
                IOChannelRef pNewChannel;
                try(IOChannel_Dup(pCurChannel, &pNewChannel));
                ObjectArray_Add(&pChildProc->ioChannels, (ObjectRef) pNewChannel);
            } else {
                ObjectArray_Add(&pChildProc->ioChannels, NULL);
            }
        }
    }

    if (pArgs->root_dir && pArgs->root_dir[0] != '\0') {
        try(Process_SetRootDirectoryPath(pChildProc, pArgs->root_dir));
    }
    if (pArgs->cw_dir && pArgs->cw_dir[0] != '\0') {
        try(Process_SetCurrentWorkingDirectoryPath(pChildProc, pArgs->cw_dir));
    }

    try(Process_AdoptChild_Locked(pProc, pChildProc->pid));
    try(Process_Exec_Locked(pChildProc, pArgs->execbase, pArgs->argv, pArgs->envp));

    try(ProcessManager_Register(gProcessManager, pChildProc));
    Object_Release(pChildProc);

    Lock_Unlock(&pProc->lock);

    if (pOutChildPid) {
        *pOutChildPid = pChildProc->pid;
    }

    return EOK;

catch:
    if (pChildProc) {
        Process_AbandonChild_Locked(pProc, pChildProc->pid);
    }
    if (needsUnlock) {
        Lock_Unlock(&pProc->lock);
    }

    Object_Release(pChildProc);

    if (pOutChildPid) {
        *pOutChildPid = 0;
    }
    return err;
}

static ssize_t calc_size_of_arg_table(const char* const _Nullable * _Nullable pTable, ssize_t maxByteCount, int* _Nonnull pOutTableEntryCount)
{
    ssize_t nbytes = 0;
    int count = 0;

    if (pTable != NULL) {
        while (*pTable != NULL) {
            const char* pStr = *pTable;

            nbytes += sizeof(char*);
            while (*pStr != '\0' && nbytes <= maxByteCount) {
                pStr++;
            }
            nbytes += 1;    // space for terminating '\0'

            if (nbytes > maxByteCount) {
                break;
            }

            pTable++;
        }
        count++;
    }
    *pOutTableEntryCount = count;

    return nbytes;
}

static errno_t Process_CopyInProcessArguments_Locked(ProcessRef _Nonnull pProc, const char* const _Nullable * _Nullable pArgv, const char* const _Nullable * _Nullable pEnv)
{
    decl_try_err();
    int nArgvCount = 0;
    int nEnvCount = 0;
    const ssize_t nbytes_argv = calc_size_of_arg_table(pArgv, __ARG_MAX, &nArgvCount);
    const ssize_t nbytes_envp = calc_size_of_arg_table(pEnv, __ARG_MAX, &nEnvCount);
    const ssize_t nbytes_argv_envp = nbytes_argv + nbytes_envp;
    if (nbytes_argv_envp > __ARG_MAX) {
        return E2BIG;
    }

    const ssize_t nbytes_procargs = __Ceil_PowerOf2(sizeof(ProcessArguments) + nbytes_argv_envp, CPU_PAGE_SIZE);
    try(AddressSpace_Allocate(pProc->addressSpace, nbytes_procargs, (void**)&pProc->argumentsBase));

    ProcessArguments* pProcArgs = (ProcessArguments*) pProc->argumentsBase;
    char** pProcArgv = (char**)(pProc->argumentsBase + sizeof(ProcessArguments));
    char** pProcEnv = (char**)&pProcArgv[nArgvCount + 1];
    char*  pDst = (char*)&pProcEnv[nEnvCount + 1];
    const char** pSrcArgv = (const char**) pArgv;
    const char** pSrcEnv = (const char**) pEnv;


    // Argv
    for (int i = 0; i < nArgvCount; i++) {
        const char* pSrc = (const char*)pSrcArgv[i];

        pProcArgv[i] = pDst;
        pDst = String_Copy(pDst, pSrc);
    }
    pProcArgv[nArgvCount] = NULL;


    // Envp
    for (int i = 0; i < nEnvCount; i++) {
        const char* pSrc = (const char*)pSrcEnv[i];

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
    pProcArgs->urt_funcs = gUrtFuncTable;

    return EOK;

catch:
    return err;
}

// Loads an executable from the given executable file into the process address
// space.
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be located at the address 'pExecAddr'
errno_t Process_Exec_Locked(ProcessRef _Nonnull pProc, Byte* _Nonnull pExecAddr, const char* const _Nullable * _Nullable pArgv, const char* const _Nullable * _Nullable pEnv)
{
    GemDosExecutableLoader loader;
    Byte* pEntryPoint = NULL;
    decl_try_err();

    // XXX for now to keep loading simpler
    assert(pProc->imageBase == NULL);

    // Copy the process arguments into the process address space
    try(Process_CopyInProcessArguments_Locked(pProc, pArgv, pEnv));

    // Load the executable
    GemDosExecutableLoader_Init(&loader, pProc->addressSpace);
    try(GemDosExecutableLoader_Load(&loader, pExecAddr, &pProc->imageBase, &pEntryPoint));
    GemDosExecutableLoader_Deinit(&loader);

    ((ProcessArguments*) pProc->argumentsBase)->image_base = pProc->imageBase;

    try(DispatchQueue_DispatchAsync(pProc->mainDispatchQueue, DispatchQueueClosure_MakeUser((Closure1Arg_Func)pEntryPoint, pProc->argumentsBase)));

    return EOK;

catch:
    return err;
}

// Adopts the process with the given PID as a child. The ppid of 'pOtherProc' must
// be the PID of the receiver.
errno_t Process_AdoptChild_Locked(ProcessRef _Nonnull pProc, ProcessId childPid)
{
    return IntArray_Add(&pProc->childPids, childPid);
}

// Abandons the process with the given PID as a child of the receiver.
void Process_AbandonChild_Locked(ProcessRef _Nonnull pProc, ProcessId childPid)
{
    IntArray_Remove(&pProc->childPids, childPid);
}

errno_t Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure)
{
    return DispatchQueue_DispatchAsync(pProc->mainDispatchQueue, DispatchQueueClosure_MakeUser(pUserClosure, NULL));
}

// Allocates more (user) address space to the given process.
errno_t Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ssize_t count, void* _Nullable * _Nonnull pOutMem)
{
    return AddressSpace_Allocate(pProc->addressSpace, count, pOutMem);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: IOChannels / Descriptors
////////////////////////////////////////////////////////////////////////////////

// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
static errno_t Process_RegisterIOChannel_Locked(ProcessRef _Nonnull pProc, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor)
{
    decl_try_err();

    // Find the lowest descriptor id that is available
    int fd = ObjectArray_GetCount(&pProc->ioChannels);
    bool hasFoundSlot = false;
    for (int i = 0; i < fd; i++) {
        if (ObjectArray_GetAt(&pProc->ioChannels, i) == NULL) {
            fd = i;
            hasFoundSlot = true;
            break;
        }
    }


    // Expand the descriptor table if we didn't find an empty slot
    if (hasFoundSlot) {
        ObjectArray_ReplaceAt(&pProc->ioChannels, (ObjectRef) pChannel, fd);
    } else {
        try(ObjectArray_Add(&pProc->ioChannels, (ObjectRef) pChannel));
    }

    *pOutDescriptor = fd;
    return EOK;

catch:
    *pOutDescriptor = -1;
    return err;
}

// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
errno_t Process_RegisterIOChannel(ProcessRef _Nonnull pProc, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = Process_RegisterIOChannel_Locked(pProc, pChannel, pOutDescriptor);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Unregisters the I/O channel identified by the given descriptor. The channel
// is removed from the process' I/O channel table and a strong reference to the
// channel is returned. The caller should call close() on the channel to close
// it and then release() to release the strong reference to the channel. Closing
// the channel will mark itself as done and the channel will be deallocated once
// the last strong reference to it has been released.
errno_t Process_UnregisterIOChannel(ProcessRef _Nonnull pProc, int fd, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    Lock_Lock(&pProc->lock);

    if (fd < 0 || fd >= ObjectArray_GetCount(&pProc->ioChannels) || ObjectArray_GetAt(&pProc->ioChannels, fd) == NULL) {
        throw(EBADF);
    }

    *pOutChannel = (IOChannelRef) ObjectArray_ExtractOwnershipAt(&pProc->ioChannels, fd);
    Lock_Unlock(&pProc->lock);

    return EOK;

catch:
    Lock_Unlock(&pProc->lock);
    *pOutChannel = NULL;
    return err;
}

// Closes all registered I/O channels. Ignores any errors that may be returned
// from the close() call of a channel.
void Process_CloseAllIOChannels_Locked(ProcessRef _Nonnull pProc)
{
    for (int i = 0; i < ObjectArray_GetCount(&pProc->ioChannels); i++) {
        IOChannelRef pChannel = (IOChannelRef) ObjectArray_GetAt(&pProc->ioChannels, i);

        if (pChannel) {
            IOChannel_Close(pChannel);
        }
    }
}

// Looks up the I/O channel identified by the given descriptor and returns a
// strong reference to it if found. The caller should call release() on the
// channel once it is no longer needed.
errno_t Process_CopyIOChannelForDescriptor(ProcessRef _Nonnull pProc, int fd, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();

    Lock_Lock(&pProc->lock);
    
    if (fd < 0 || fd >= ObjectArray_GetCount(&pProc->ioChannels)) {
        throw(EBADF);
    }

    IOChannelRef pChannel = (IOChannelRef) ObjectArray_GetAt(&pProc->ioChannels, fd);
    if (pChannel == NULL) {
        throw(EBADF);
    }

    *pOutChannel = Object_RetainAs(pChannel, IOChannel);
    Lock_Unlock(&pProc->lock);

    return EOK;

catch:
    Lock_Unlock(&pProc->lock);
    *pOutChannel = NULL;
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: File I/O
////////////////////////////////////////////////////////////////////////////////

// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
errno_t Process_SetRootDirectoryPath(ProcessRef _Nonnull pProc, const char* pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_SetRootDirectoryPath(&pProc->pathResolver, pProc->realUser, pPath);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Sets the receiver's current working directory to the given path.
errno_t Process_SetCurrentWorkingDirectoryPath(ProcessRef _Nonnull pProc, const char* _Nonnull pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_SetCurrentWorkingDirectoryPath(&pProc->pathResolver, pProc->realUser, pPath);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
errno_t Process_GetCurrentWorkingDirectoryPath(ProcessRef _Nonnull pProc, char* _Nonnull pBuffer, ssize_t bufferSize)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_GetCurrentWorkingDirectoryPath(&pProc->pathResolver, pProc->realUser, pBuffer, bufferSize);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
FilePermissions Process_GetFileCreationMask(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const FilePermissions mask = pProc->fileCreationMask;
    Lock_Unlock(&pProc->lock);
    return mask;
}

// Sets the file creation mask of the receiver.
void Process_SetFileCreationMask(ProcessRef _Nonnull pProc, FilePermissions mask)
{
    Lock_Lock(&pProc->lock);
    pProc->fileCreationMask = mask & 0777;
    Lock_Unlock(&pProc->lock);
}

// Creates a file in the given filesystem location.
errno_t Process_CreateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, FilePermissions permissions, int* _Nonnull pOutDescriptor)
{
    decl_try_err();
    PathResolverResult r;
    InodeRef _Locked pFileNode = NULL;
    FileRef pFile = NULL;

    Lock_Lock(&pProc->lock);

    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r));
    try(Filesystem_CreateFile(r.filesystem, &r.lastPathComponent, r.inode, pProc->realUser, options, ~pProc->fileCreationMask & (permissions & 0777), &pFileNode));
    try(IOResource_Open(r.filesystem, pFileNode, options, pProc->realUser, (IOChannelRef*)&pFile));
    try(Process_RegisterIOChannel_Locked(pProc, (IOChannelRef)pFile, pOutDescriptor));

catch:
    if (pFileNode) {
        Filesystem_RelinquishNode(r.filesystem, pFileNode);
    }
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t Process_Open(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, int* _Nonnull pOutDescriptor)
{
    decl_try_err();
    PathResolverResult r;
    FileRef pFile;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(IOResource_Open(r.filesystem, r.inode, options, pProc->realUser, (IOChannelRef*)&pFile));
    try(Process_RegisterIOChannel_Locked(pProc, (IOChannelRef)pFile, pOutDescriptor));
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    Object_Release(pFile);
    *pOutDescriptor = -1;
    return err;
}

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
errno_t Process_CreateDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FilePermissions permissions)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);

    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r));
    try(Filesystem_CreateDirectory(r.filesystem, &r.lastPathComponent, r.inode, pProc->realUser, ~pProc->fileCreationMask & (permissions & 0777)));

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
errno_t Process_OpenDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, int* _Nonnull pOutDescriptor)
{
    decl_try_err();
    PathResolverResult r;
    DirectoryRef pDir = NULL;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(Filesystem_OpenDirectory(r.filesystem, r.inode, pProc->realUser, &pDir));
    try(Process_RegisterIOChannel_Locked(pProc, (IOChannelRef)pDir, pOutDescriptor));
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    Object_Release(pDir);
    *pOutDescriptor = -1;
    return err;
}

// Returns information about the file at the given path.
errno_t Process_GetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(Filesystem_GetFileInfo(r.filesystem, r.inode, pOutInfo));

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_GetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int fd, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel));
    if (Object_InstanceOf(pChannel, File)) {
        try(Filesystem_GetFileInfo(File_GetFilesystem(pChannel), File_GetInode(pChannel), pOutInfo));
    }
    else if (Object_InstanceOf(pChannel, Directory)) {
        try(Filesystem_GetFileInfo(Directory_GetFilesystem(pChannel), Directory_GetInode(pChannel), pOutInfo));
    }
    else {
        throw(EBADF);
    }

catch:
    Object_Release(pChannel);
    return err;
}

// Modifies information about the file at the given path.
errno_t Process_SetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(Filesystem_SetFileInfo(r.filesystem, r.inode, pProc->realUser, pInfo));

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_SetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int fd, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel));
    if (Object_InstanceOf(pChannel, File)) {
        try(Filesystem_SetFileInfo(File_GetFilesystem(pChannel), File_GetInode(pChannel), pProc->realUser, pInfo));
    }
    else if (Object_InstanceOf(pChannel, Directory)) {
        try(Filesystem_SetFileInfo(Directory_GetFilesystem(pChannel), Directory_GetInode(pChannel), pProc->realUser, pInfo));
    }
    else {
        throw(EBADF);
    }

catch:
    Object_Release(pChannel);
    return err;
}

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
errno_t Process_TruncateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileOffset length)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(Filesystem_Truncate(r.filesystem, r.inode, pProc->realUser, length));

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Same as above but the file is identified by the given I/O channel.
errno_t Process_TruncateFileFromIOChannel(ProcessRef _Nonnull pProc, int fd, FileOffset length)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel));
    if (Object_InstanceOf(pChannel, File)) {
        try(Filesystem_Truncate(File_GetFilesystem(pChannel), File_GetInode(pChannel), pProc->realUser, length));
    }
    else if (Object_InstanceOf(pChannel, Directory)) {
        throw(EISDIR);
    }
    else {
        throw(ENOTDIR);
    }

catch:
    Object_Release(pChannel);
    return err;
}

// Sends a I/O Channel or I/O Resource defined command to the I/O Channel or
// resource identified by the given descriptor.
errno_t Process_vIOControl(ProcessRef _Nonnull pProc, int fd, int cmd, va_list ap)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel));
    err = IOChannel_vIOControl(pChannel, cmd, ap);

catch:
    Object_Release(pChannel);
    return err;
}

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
errno_t Process_CheckFileAccess(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, int mode)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    if (mode != 0) {
        try(Filesystem_CheckAccess(r.filesystem, r.inode, pProc->realUser, mode));
    }

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Unlinks the inode at the path 'pPath'.
errno_t Process_Unlink(ProcessRef _Nonnull pProc, const char* _Nonnull pPath)
{
    decl_try_err();
    PathResolverResult r;
    InodeRef _Locked pSecondNode = NULL;
    InodeRef _Weak _Locked pParentNode = NULL;
    InodeRef _Weak _Locked pNodeToUnlink = NULL;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r));


    // Get the inode of the file/directory to unlink. Note that there are two cases here:
    // unlink("."): we need to grab the parent of the directory and make r.inode the node to unlink
    // unlink("anything_else"): r.inode is our parent and we look up the target
    if (r.lastPathComponent.count == 1 && r.lastPathComponent.name[0] == '.') {
        try(Filesystem_AcquireNodeForName(r.filesystem, r.inode, &kPathComponent_Parent, pProc->realUser, &pSecondNode));
        pNodeToUnlink = r.inode;
        pParentNode = pSecondNode;
    }
    else {
        try(Filesystem_AcquireNodeForName(r.filesystem, r.inode, &r.lastPathComponent, pProc->realUser, &pSecondNode));
        pNodeToUnlink = pSecondNode;
        pParentNode = r.inode;
    }


    // Can not unlink a mountpoint
    if (FilesystemManager_IsNodeMountpoint(gFilesystemManager, pNodeToUnlink)) {
        throw(EBUSY);
    }


    // Can not unlink the root of a filesystem
    if (Inode_IsDirectory(pNodeToUnlink) && Inode_GetId(pNodeToUnlink) == Inode_GetId(pParentNode)) {
        throw(EBUSY);
    }


    // Can not unlink the process' root directory
    if (PathResolver_IsRootDirectory(&pProc->pathResolver, pNodeToUnlink)) {
        throw(EBUSY);
    }

    try(Filesystem_Unlink(r.filesystem, pNodeToUnlink, pParentNode, pProc->realUser));

catch:
    if (pSecondNode) {
        Filesystem_RelinquishNode(r.filesystem, pSecondNode);
    }
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
errno_t Process_Rename(ProcessRef _Nonnull pProc, const char* pOldPath, const char* pNewPath)
{
    decl_try_err();
    PathResolverResult or, nr;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pOldPath, pProc->realUser, &or));
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pNewPath, pProc->realUser, &nr));

    // Can not rename a mount point
    // XXX implement this check once we've refined the mount point handling (return EBUSY)

    // newpath and oldpath have to be in the same filesystem
    // XXX implement me

    // newpath can't be a child of oldpath
    // XXX implement me

    // unlink the target node if one exists for newpath
    // XXX implement me
    
    try(Filesystem_Rename(or.filesystem, &or.lastPathComponent, or.inode, &nr.lastPathComponent, nr.inode, pProc->realUser));

catch:
    PathResolverResult_Deinit(&or);
    PathResolverResult_Deinit(&nr);
    Lock_Unlock(&pProc->lock);
    return err;
}
