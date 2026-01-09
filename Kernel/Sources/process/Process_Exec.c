//
//  Process_Exec.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "proc_img_gemdos.h"
#include <assert.h>
#include <ext/string.h>
#include <kei/kei.h>
#include <sched/vcpu.h>


static ssize_t calc_size_of_arg_table(const char* const _Nullable table[], size_t* _Nonnull pOutCount)
{
    ssize_t nbytes = 0;
    size_t count = 0;

    while (table[count]) {
        const char* pa = table[count];
        const ssize_t slen = strnlen_s(pa, __ARG_STRLEN_MAX);
        const ssize_t nbytes_entry = sizeof(char*) + slen + 1;

        if (pa[slen] != '\0') {
            return -1;
        }
        if ((nbytes + nbytes_entry) > __ARG_MAX) {
            return -1;
        }

        nbytes += nbytes_entry;
        count++;
    }
    *pOutCount = count;

    return nbytes;
}

static errno_t _proc_img_copy_args_env(proc_img_t* _Nonnull pimg, const char* argv[], const char* _Nullable env[])
{
    size_t nArgvCount = 0;
    size_t nEnvCount = 0;
    const ssize_t nbytes_argv = calc_size_of_arg_table(argv, &nArgvCount);
    const ssize_t nbytes_envp = calc_size_of_arg_table(env, &nEnvCount);
    const ssize_t nbytes_argv_envp = nbytes_argv + nbytes_envp;

    if (nbytes_argv < 0 || nbytes_envp < 0 || nbytes_argv_envp > __ARG_MAX) {
        return E2BIG;
    }

    pargs_t* pargs = NULL;
    const ssize_t nbytes_procargs = __Ceil_PowerOf2(sizeof(pargs_t) + nbytes_argv_envp, CPU_PAGE_SIZE);
    const errno_t err = AddressSpace_Allocate(&pimg->as, nbytes_procargs, (void**)&pargs);
    if (err != EOK) {
        return err;
    }


    char** proc_argv = (char**)((char*)pargs + sizeof(pargs_t));
    char** proc_env = (char**)&proc_argv[nArgvCount + 1];
    char*  dst = (char*)&proc_env[nEnvCount + 1];


    // Argv
    for (size_t i = 0; i < nArgvCount; i++) {
        proc_argv[i] = dst;
        dst = strcpy_x(dst, argv[i]) + 1;
    }
    proc_argv[nArgvCount] = NULL;


    // Envp
    for (size_t i = 0; i < nEnvCount; i++) {
        proc_env[i] = dst;
        dst = strcpy_x(dst, env[i]) + 1;
    }
    proc_env[nEnvCount] = NULL;


    // Process Arguments
    pargs->version = sizeof(pargs_t);
    pargs->reserved = 0;
    pargs->arguments_size = nbytes_procargs;
    pargs->argc = nArgvCount;
    pargs->argv = proc_argv;
    pargs->envp = proc_env;
    pargs->image_base = NULL;
    pargs->urt_funcs = gKeiTable;

    pimg->pargs = (char*)pargs;

    return EOK;
}

static errno_t _proc_img_acquire_main_vcpu(vcpu_func_t _Nonnull entryPoint, void* _Nonnull procargs, vcpu_t _Nonnull * _Nullable pOutVcpu)
{
    decl_try_err();
    vcpu_t vp = NULL;
    vcpu_acquisition_t ac;

    ac.func = (VoidFunc_1)entryPoint;
    ac.arg = procargs;
    ac.ret_func = (VoidFunc_0)vcpu_uret_exit;
    ac.kernelStackBase = NULL;
    ac.kernelStackSize = 0;
    ac.userStackSize = PROC_DEFAULT_USER_STACK_SIZE;
    ac.id = VCPUID_MAIN;
    ac.groupid = VCPUID_MAIN_GROUP;
    ac.schedParams.type = SCHED_PARAM_QOS;
    ac.schedParams.u.qos.category = SCHED_QOS_INTERACTIVE;
    ac.schedParams.u.qos.priority = QOS_PRI_NORMAL;
    ac.isUser = true;

    err = vcpu_acquire(&ac, &vp);

    *pOutVcpu = vp;
    return err;
}

// Loads an executable from the given executable file into the process address
// space.
// \param self the process into which the executable image should be loaded
// \param path path to the executable file
// \param argv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param env the environment for the process. Null means that the process inherits the environment from its parent
// XXX the executable format is GemDOS
static errno_t _proc_build_exec_image(ProcessRef _Nonnull _Locked self, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[], proc_img_t* _Nonnull pimg)
{
    decl_try_err();
    IOChannelRef chan = NULL;
    const char* null_sptr[1] = {NULL};

    if (argv == NULL) {
        argv = null_sptr;
    }
    if (env == NULL) {
        env = null_sptr;
    }


    // Open the executable file and lock it
    try(FileManager_OpenFile(&self->fm, path, O_RDONLY | _O_EXONLY, &chan));


    // Copy the process arguments into the process address space
    try(_proc_img_copy_args_env(pimg, argv, env));


    // Load the executable
    try(_proc_img_load_gemdos_exec(pimg, (InodeChannelRef)chan));
    ((pargs_t*)(pimg->pargs))->image_base = pimg->base;


    // Create the new main vcpu
    try(_proc_img_acquire_main_vcpu((vcpu_func_t)pimg->entry_point, pimg->pargs, &pimg->main_vp));


catch:
    IOChannel_Release(chan);

    return err;
}

static void _proc_img_deactivate_current(ProcessRef _Nonnull self)
{
    if (deque_empty(&self->vcpu_queue)) {
        return;
    }


    deque_remove(&self->vcpu_queue, &(vcpu_current()->owner_qe));
    self->vcpu_count--;
    _proc_abort_other_vcpus(self);

    mtx_unlock(&self->mtx);
    _proc_reap_vcpus(self);
    mtx_lock(&self->mtx);

    _proc_destroy_sigroutes(self);
    IOChannelTable_ReleaseExecChannels(&self->ioChannelTable);
}

static void _proc_img_activate(ProcessRef _Nonnull self, const proc_img_t* _Nonnull pimg)
{
    AddressSpace_AdoptMappingsFrom(&self->addr_space, &pimg->as);
    deque_add_last(&self->vcpu_queue, &pimg->main_vp->owner_qe);
    self->vcpu_count++;
    pimg->main_vp->proc = self;
    self->pargs_base = pimg->pargs;
}

errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[], bool resumed)
{
    decl_try_err();
    proc_img_t pimg = (proc_img_t){0};

    AddressSpace_Init(&pimg.as);

    mtx_lock(&self->mtx);

    // We only permit calling Process_Exit() from another process if that other
    // process is building us (thus there's no vcpu assigned to 'self' at this
    // point).
    assert(deque_empty(&self->vcpu_queue)
        || (!deque_empty(&self->vcpu_queue) && vcpu_current()->proc == self));

    
    // Don't do an exec() if we are in the process of being shut down
    if (vcpu_aborting(vcpu_current())) {
        throw(EINTR);
    }


    // Create the new exec image
    try(_proc_build_exec_image(self, execPath, argv, env, &pimg));

    
    // We now got:
    // - a new address space with the executable image mapped in
    // - a new vcpu suitable to act as a main vcpu
    // we'll now demolish the existing executable image and install the new
    // address map and main vcpu
    _proc_img_deactivate_current(self);
    _proc_img_activate(self, &pimg);


catch:
    mtx_unlock(&self->mtx);

    AddressSpace_Deinit(&pimg.as);


    if (resumed) {
        vcpu_resume(pimg.main_vp, false);
    }

    return err;
}

void Process_ResumeMainVirtualProcessor(ProcessRef _Nonnull self)
{
    vcpu_resume(vcpu_from_owner_qe(self->vcpu_queue.first), false);
}