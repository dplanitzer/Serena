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


errno_t Process_SpawnChildProcess(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, const SpawnOptions* _Nonnull pOptions, ProcessId * _Nullable pOutChildPid)
{
    decl_try_err();
    ProcessRef pChildProc = NULL;
    bool needsUnlock = false;

    Lock_Lock(&pProc->lock);
    needsUnlock = true;

    const FilePermissions childUMask = ((pOptions->options & kSpawn_OverrideUserMask) != 0) ? (pOptions->umask & 0777) : pProc->fileCreationMask;
    try(Process_Create(pProc->pid, pProc->realUser, pProc->rootDirectory, pProc->workingDirectory, pProc->fileCreationMask, &pChildProc));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

    IOChannelTable_CopyFrom(&pChildProc->ioChannelTable, &pProc->ioChannelTable);

    if (pOptions->root_dir && pOptions->root_dir[0] != '\0') {
        try(Process_SetRootDirectoryPath(pChildProc, pOptions->root_dir));
    }
    if (pOptions->cw_dir && pOptions->cw_dir[0] != '\0') {
        try(Process_SetWorkingDirectoryPath(pChildProc, pOptions->cw_dir));
    }

    try(Process_AdoptChild_Locked(pProc, pChildProc->pid));
    try(Process_Exec_Locked(pChildProc, pPath, pOptions->argv, pOptions->envp));

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

static ssize_t calc_size_of_arg_table(const char* const _Nullable * _Nullable pTable, ssize_t maxByteCount, size_t* _Nonnull pOutTableEntryCount)
{
    ssize_t nbytes = 0;
    size_t i = 0;

    if (pTable) {
        while (pTable[i]) {
            const char* pStr = pTable[i];

            nbytes += sizeof(char*);
            while (*pStr != '\0' && nbytes <= maxByteCount) {
                pStr++;
                nbytes++;
            }
            nbytes++;   // terminating '\0'

            if (nbytes > maxByteCount) {
                break;
            }

            i++;
        }
    }
    *pOutTableEntryCount = i;

    return nbytes;
}

static errno_t Process_CopyInProcessArguments_Locked(ProcessRef _Nonnull pProc, const char* const _Nullable * _Nullable pArgv, const char* const _Nullable * _Nullable pEnv)
{
    decl_try_err();
    size_t nArgvCount = 0;
    size_t nEnvCount = 0;
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
    for (size_t i = 0; i < nArgvCount; i++) {
        const char* pSrc = (const char*)pSrcArgv[i];

        pProcArgv[i] = pDst;
        pDst = String_Copy(pDst, pSrc);
    }
    pProcArgv[nArgvCount] = NULL;


    // Envp
    for (size_t i = 0; i < nEnvCount; i++) {
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
static errno_t Process_LoadExecutableImage_Locked(ProcessRef _Nonnull pProc, const char* _Nonnull pExecPath, void** pImageBase, void** pEntryPoint)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    GemDosExecutableLoader loader;

    GemDosExecutableLoader_Init(&loader, pProc->addressSpace, pProc->realUser);
    Process_MakePathResolver(pProc, &pr);

    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pExecPath, &r));

    Inode_Lock(r.inode); 
    if (Inode_IsRegularFile(r.inode)) {
        err = Filesystem_CheckAccess(Inode_GetFilesystem(r.inode), r.inode, pProc->realUser, kAccess_Readable | kAccess_Executable);
        if (err == EOK) {
            err = GemDosExecutableLoader_Load(&loader, r.inode, pImageBase, pEntryPoint);
        }
    }
    else {
        // Not a regular file
        err = EACCESS;
    }
    Inode_Unlock(r.inode);

catch:
    GemDosExecutableLoader_Deinit(&loader);
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    return err;
}

// Loads an executable from the given executable file into the process address
// space.
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
errno_t Process_Exec_Locked(ProcessRef _Nonnull pProc, const char* _Nonnull pExecPath, const char* const _Nullable * _Nullable pArgv, const char* const _Nullable * _Nullable pEnv)
{
    decl_try_err();
    void* pImageBase = NULL;
    void* pEntryPoint = NULL;

    // XXX for now to keep loading simpler
    assert(pProc->imageBase == NULL);


    // Copy the process arguments into the process address space
    try(Process_CopyInProcessArguments_Locked(pProc, pArgv, pEnv));


    // Load the executable
    try(Process_LoadExecutableImage_Locked(pProc, pExecPath, &pImageBase, &pEntryPoint));

    pProc->imageBase = pImageBase;
    ((ProcessArguments*) pProc->argumentsBase)->image_base = pProc->imageBase;


    // Dispatch the invocation of the entry point
    try(DispatchQueue_DispatchAsync(pProc->mainDispatchQueue,
        DispatchQueueClosure_MakeUser((Closure1Arg_Func)pEntryPoint, pProc->argumentsBase)));

catch:
    //XXX free the executable image if an error occurred
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
