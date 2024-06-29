//
//  Process_Exec.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "GemDosExecutableLoader.h"
#include <krt/krt.h>


static ssize_t calc_size_of_arg_table(const char* const _Nullable table[], ssize_t maxByteCount, size_t* _Nonnull pOutTableEntryCount)
{
    ssize_t nbytes = 0;
    size_t i = 0;

    while (table[i]) {
        const char* pStr = table[i];

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
    *pOutTableEntryCount = i;

    return nbytes;
}

static errno_t Process_CopyInProcessArguments_Locked(ProcessRef _Nonnull self, const char* argv[], const char* _Nullable env[])
{
    decl_try_err();
    size_t nArgvCount = 0;
    size_t nEnvCount = 0;
    const ssize_t nbytes_argv = calc_size_of_arg_table(argv, __ARG_MAX, &nArgvCount);
    const ssize_t nbytes_envp = calc_size_of_arg_table(env, __ARG_MAX, &nEnvCount);
    const ssize_t nbytes_argv_envp = nbytes_argv + nbytes_envp;
    
    if (nbytes_argv_envp > __ARG_MAX) {
        return E2BIG;
    }

    const ssize_t nbytes_procargs = __Ceil_PowerOf2(sizeof(ProcessArguments) + nbytes_argv_envp, CPU_PAGE_SIZE);
    try(AddressSpace_Allocate(self->addressSpace, nbytes_procargs, (void**)&self->argumentsBase));

    ProcessArguments* pProcArgs = (ProcessArguments*) self->argumentsBase;
    char** pProcArgv = (char**)(self->argumentsBase + sizeof(ProcessArguments));
    char** pProcEnv = (char**)&pProcArgv[nArgvCount + 1];
    char*  pDst = (char*)&pProcEnv[nEnvCount + 1];
    const char** pSrcArgv = (const char**) argv;
    const char** pSrcEnv = (const char**) env;


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

catch:
    return err;
}

// Loads an executable from the given executable file into the process address
// space.
static errno_t Process_LoadExecutableImage_Locked(ProcessRef _Nonnull self, const char* _Nonnull pExecPath, void** pImageBase, void** pEntryPoint)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    GemDosExecutableLoader loader;

    GemDosExecutableLoader_Init(&loader, self->addressSpace, self->realUser);
    Process_MakePathResolver(self, &pr);

    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pExecPath, &r));

    Inode_Lock(r.inode); 
    if (Inode_IsRegularFile(r.inode)) {
        err = Filesystem_CheckAccess(Inode_GetFilesystem(r.inode), r.inode, self->realUser, kAccess_Readable | kAccess_Executable);
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
errno_t Process_Exec_Locked(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    void* pImageBase = NULL;
    void* pEntryPoint = NULL;
    const char* null_sptr[1] = {NULL};

    if (argv == NULL) {
        argv = null_sptr;
    }
    if (env == NULL) {
        env = null_sptr;
    }

    // XXX for now to keep loading simpler
    assert(self->imageBase == NULL);


    // Copy the process arguments into the process address space
    try(Process_CopyInProcessArguments_Locked(self, argv, env));


    // Load the executable
    try(Process_LoadExecutableImage_Locked(self, path, &pImageBase, &pEntryPoint));

    self->imageBase = pImageBase;
    ((ProcessArguments*) self->argumentsBase)->image_base = self->imageBase;


    // Dispatch the invocation of the entry point
    try(DispatchQueue_DispatchAsync(self->mainDispatchQueue,
        DispatchQueueClosure_MakeUser((Closure1Arg_Func)pEntryPoint, self->argumentsBase)));

catch:
    //XXX free the executable image if an error occurred
    return err;
}
