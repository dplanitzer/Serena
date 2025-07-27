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

static errno_t _proc_img_copy_args_env(AddressSpaceRef _Nonnull as, const char* argv[], const char* _Nullable env[], pargs_t* _Nonnull * _Nullable procargs)
{
    decl_try_err();
    size_t nArgvCount = 0;
    size_t nEnvCount = 0;
    const ssize_t nbytes_argv = calc_size_of_arg_table(argv, __ARG_MAX, &nArgvCount);
    const ssize_t nbytes_envp = calc_size_of_arg_table(env, __ARG_MAX, &nEnvCount);
    const ssize_t nbytes_argv_envp = nbytes_argv + nbytes_envp;
    pargs_t* pargs = NULL;

    if (nbytes_argv_envp > __ARG_MAX) {
        return E2BIG;
    }


    const ssize_t nbytes_procargs = __Ceil_PowerOf2(sizeof(pargs_t) + nbytes_argv_envp, CPU_PAGE_SIZE);
    err = AddressSpace_Allocate(as, nbytes_procargs, (void**)&pargs);
    if (err != EOK) {
        return err;
    }


    char** pProcArgv = (char**)((char*)pargs + sizeof(pargs_t));
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
    pargs->version = sizeof(pargs_t);
    pargs->reserved = 0;
    pargs->arguments_size = nbytes_procargs;
    pargs->argc = nArgvCount;
    pargs->argv = pProcArgv;
    pargs->envp = pProcEnv;
    pargs->image_base = NULL;
    pargs->urt_funcs = gKeiTable;

    *procargs = pargs;

    return err;
}

static errno_t _proc_img_acquire_main_vcpu(vcpu_func_t _Nonnull entryPoint, void* _Nonnull procargs, vcpu_t _Nonnull * _Nullable pOutVcpu)
{
    decl_try_err();
    vcpu_t vp = NULL;
    VirtualProcessorParameters kp;

    kp.func = (VoidFunc_1)entryPoint;
    kp.context = procargs;
    kp.ret_func = (VoidFunc_0)vcpu_uret_exit;
    kp.kernelStackSize = VP_DEFAULT_KERNEL_STACK_SIZE;
    kp.userStackSize = VP_DEFAULT_USER_STACK_SIZE;
    kp.id = VCPUID_MAIN;
    kp.groupid = VCPUID_MAIN_GROUP;
    kp.priority = kDispatchQoS_Interactive * kDispatchPriority_Count + (kDispatchPriority_Normal + kDispatchPriority_Count / 2) + VP_PRIORITIES_RESERVED_LOW;
    kp.isUser = true;

    err = vcpu_pool_acquire(g_vcpu_pool, &kp, &vp);

    *pOutVcpu = vp;
    return err;
}

// Loads an executable from the given executable file into the process address
// space.
static errno_t _proc_img_load_exec_file(AddressSpaceRef _Nonnull as, FileChannelRef _Locked chan, void** pImageBase, void** pEntryPoint)
{
    decl_try_err();
    GemDosExecutableLoader loader;

    GemDosExecutableLoader_Init(&loader, as);
    err = GemDosExecutableLoader_Load(&loader, chan, pImageBase, pEntryPoint);
    GemDosExecutableLoader_Deinit(&loader);

    return err;
}

// Loads an executable from the given executable file into the process address
// space.
// \param self the process into which the executable image should be loaded
// \param path path to the executable file
// \param argv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param env the environment for the process. Null means that the process inherits the environment from its parent
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
static errno_t _proc_build_exec_image(ProcessRef _Locked _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    IOChannelRef chan = NULL;
    void* imageBase = NULL;
    void* entryPoint = NULL;
    pargs_t* pargs = NULL;
    const char* null_sptr[1] = {NULL};
    AddressSpace as;

    if (argv == NULL) {
        argv = null_sptr;
    }
    if (env == NULL) {
        env = null_sptr;
    }

    // XXX for now to keep loading simpler
    assert(self->imageBase == NULL);


    // Create a new address space which we'll use to build the new process image
    AddressSpace_Init(&as);

    
    // Open the executable file and lock it
    try(FileManager_OpenExecutable(&self->fm, path, &chan));


    // Copy the process arguments into the process address space
    try(_proc_img_copy_args_env(&as, argv, env, &pargs));


    // Load the executable
    try(_proc_img_load_exec_file(&as, (FileChannelRef)chan, &imageBase, &entryPoint));

    pargs->image_base = imageBase;
    self->imageBase = imageBase;
    self->argumentsBase = (char*)pargs;


    // Create the new main vcpu
    vcpu_t main_vp;
    try(_proc_img_acquire_main_vcpu((vcpu_func_t)entryPoint, pargs, &main_vp));
    List_InsertAfterLast(&self->vcpu_queue, &main_vp->owner_qe);
    main_vp->proc = self;


    // Install the new memory mappings in our address space and remove the old
    // mappings
    AddressSpace_AdoptMappingsFrom(self->addressSpace, &as);


catch:
    //XXX free the executable image if an error occurred
    IOChannel_Release(chan);
    AddressSpace_Deinit(&as);

    return err;
}

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param pProc the process into which the executable image should be loaded
// \param pExecPath path to a GemDOS executable file
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
errno_t Process_BuildExecImage(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (!vcpu_aborting(vcpu_current())) {
        err = _proc_build_exec_image(self, execPath, argv, env);
    }
    else {
        err = EINTR;
    }
    mtx_unlock(&self->mtx);

    return err;
}

void Process_ResumeMainVirtualProcessor(ProcessRef _Nonnull self)
{
    vcpu_resume(VP_FROM_OWNER_NODE(self->vcpu_queue.first), false);
}