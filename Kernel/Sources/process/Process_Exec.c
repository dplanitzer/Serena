//
//  Process_Exec.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "GemDosExecutableLoader.h"
#include <kei/kei.h>
#include <kern/string.h>
#include <dispatchqueue/DispatchQueue.h>
#include <sched/vcpu.h>


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

static errno_t copy_in_args(ProcessRef _Nonnull self, const char* argv[], const char* _Nullable env[])
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

    const ssize_t nbytes_procargs = __Ceil_PowerOf2(sizeof(pargs_t) + nbytes_argv_envp, CPU_PAGE_SIZE);
    try(AddressSpace_Allocate(self->addressSpace, nbytes_procargs, (void**)&self->argumentsBase));

    pargs_t* pProcArgs = (pargs_t*) self->argumentsBase;
    char** pProcArgv = (char**)(self->argumentsBase + sizeof(pargs_t));
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
    pProcArgs->version = sizeof(pargs_t);
    pProcArgs->reserved = 0;
    pProcArgs->arguments_size = nbytes_procargs;
    pProcArgs->argc = nArgvCount;
    pProcArgs->argv = pProcArgv;
    pProcArgs->envp = pProcEnv;
    pProcArgs->image_base = NULL;
    pProcArgs->urt_funcs = gKeiTable;

catch:
    return err;
}

// Loads an executable from the given executable file into the process address
// space.
static errno_t load_gemdos_executable(ProcessRef _Nonnull self, FileChannelRef _Locked chan, void** pImageBase, void** pEntryPoint)
{
    decl_try_err();
    GemDosExecutableLoader loader;

    GemDosExecutableLoader_Init(&loader, self->addressSpace);
    err = GemDosExecutableLoader_Load(&loader, chan, pImageBase, pEntryPoint);
    GemDosExecutableLoader_Deinit(&loader);

    return err;
}

// Loads an executable from the given executable file into the process address
// space.
// \param self the process into which the executable image should be loaded
// \param pExecAddr pointer to a GemDOS formatted executable file in memory
// \param pArgv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param pEnv the environment for the process. Null means that the process inherits the environment from its parent
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
static errno_t _proc_exec(ProcessRef _Locked _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    IOChannelRef chan = NULL;
    void* imageBase = NULL;
    void* entryPoint = NULL;
    const char* null_sptr[1] = {NULL};

    if (argv == NULL) {
        argv = null_sptr;
    }
    if (env == NULL) {
        env = null_sptr;
    }

    // XXX for now to keep loading simpler
    assert(self->imageBase == NULL);


    // Open the executable file and lock it
    try(FileManager_OpenExecutable(&self->fm, path, &chan));

    // Copy the process arguments into the process address space
    try(copy_in_args(self, argv, env));


    // Load the executable
    try(load_gemdos_executable(self, (FileChannelRef)chan, &imageBase, &entryPoint));

    self->imageBase = imageBase;
    ((pargs_t*) self->argumentsBase)->image_base = self->imageBase;


    _vcpu_acquire_attr_t attr;
    attr.func = (vcpu_func_t)entryPoint;
    attr.arg = self->argumentsBase;
    attr.data = 0;
    attr.priority = kDispatchQoS_Interactive * kDispatchPriority_Count + (kDispatchPriority_Normal + kDispatchPriority_Count / 2) + VP_PRIORITIES_RESERVED_LOW;
    attr.stack_size = 0;
    attr.groupid = VCPUID_MAIN_GROUP;
    attr.flags = VCPU_ACQUIRE_RESUMED;

    vcpuid_t vid;
    try(Process_AcquireVirtualProcessor(self, &attr, &vid));

catch:
    //XXX free the executable image if an error occurred
    IOChannel_Release(chan);

    return err;
}

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param pProc the process into which the executable image should be loaded
// \param pExecPath path to a GemDOS executable file
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[])
{
//    mtx_lock(&self->mtx);
    const errno_t err = _proc_exec(self, execPath, argv, env);
//    mtx_unlock(&self->mtx);

    return err;
}
