//
//  Process_Spawn.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "GemDosExecutableLoader.h"
#include "ProcessManager.h"
#include <krt/krt.h>


errno_t Process_SpawnChildProcess(ProcessRef _Nonnull pProc, const SpawnArguments* _Nonnull pArgs, ProcessId * _Nullable pOutChildPid)
{
    decl_try_err();
    ProcessRef pChildProc = NULL;
    bool needsUnlock = false;

    Lock_Lock(&pProc->lock);
    needsUnlock = true;

    const FilePermissions childUMask = ((pArgs->options & kSpawn_OverrideUserMask) != 0) ? (pArgs->umask & 0777) : pProc->fileCreationMask;
    try(Process_Create(pProc->pid, pProc->realUser, pProc->pathResolver.rootDirectory, pProc->pathResolver.currentWorkingDirectory, pProc->fileCreationMask, &pChildProc));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

    if ((pArgs->options & kSpawn_NoDefaultDescriptors) == 0) {
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
        try(Process_SetWorkingDirectory(pChildProc, pArgs->cw_dir));
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
errno_t Process_Exec_Locked(ProcessRef _Nonnull pProc, void* _Nonnull pExecAddr, const char* const _Nullable * _Nullable pArgv, const char* const _Nullable * _Nullable pEnv)
{
    GemDosExecutableLoader loader;
    void* pEntryPoint = NULL;
    decl_try_err();

    // XXX for now to keep loading simpler
    assert(pProc->imageBase == NULL);

    // Copy the process arguments into the process address space
    try(Process_CopyInProcessArguments_Locked(pProc, pArgv, pEnv));

    // Load the executable
    GemDosExecutableLoader_Init(&loader, pProc->addressSpace);
    try(GemDosExecutableLoader_Load(&loader, pExecAddr, (void**)&pProc->imageBase, &pEntryPoint));
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
