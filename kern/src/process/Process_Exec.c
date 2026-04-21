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


// Calculates the size of the string array that will be used to store the 'argv'
// strings. This includes the NUL marking the end of each string and the final
// NUL entry that marks the end of the string array. Note this is not the argv[]
// vector that points to the string. This is a separate thing.
static ssize_t calc_size_of_arg_strings(const char* const _Nullable argv[], size_t* _Nonnull pOutCount)
{
    ssize_t nbytes = 0;
    size_t count = 0;

    while (argv[count]) {
        const char* pa = argv[count];
        const ssize_t slen = strnlen_s(pa, __ARG_STRLEN_MAX);
        const ssize_t entry_size = slen + 1;

        if (pa[slen] != '\0') {
            return -1;
        }
        if ((nbytes + entry_size) > __ARG_MAX) {
            return -1;
        }

        nbytes += entry_size;
        count++;
    }
    *pOutCount = count;
    nbytes++;   // This is for the empty string ('\0') that marks the end of teh string array

    return nbytes;
}

static errno_t _proc_img_copy_args_env(proc_img_t* _Nonnull pimg, const char* argv[], const char* _Nullable env[])
{
    size_t argc = 0;
    size_t envc = 0;
    const ssize_t arg_strings_size = calc_size_of_arg_strings(argv, &argc);
    const ssize_t env_strings_size = calc_size_of_arg_strings(env, &envc);

    if (arg_strings_size < 0 || env_strings_size < 0 || (arg_strings_size + env_strings_size) > __ARG_MAX) {
        return E2BIG;
    }

    proc_ctx_t* pctx = NULL;
    const size_t argv_size = sizeof(char*) * (argc + 1);
    const size_t envv_size = sizeof(char*) * (envc + 1);
    const size_t pure_ctx_size = sizeof(proc_ctx_t) + argv_size + arg_strings_size + envv_size + env_strings_size;
    const ssize_t ctx_size = __Ceil_PowerOf2(pure_ctx_size, CPU_PAGE_SIZE);
    const errno_t err = AddressSpace_Allocate(&pimg->as, ctx_size, (void**)&pctx);
    if (err != EOK) {
        return err;
    }


    char* p = (char*)pctx;
    p += sizeof(proc_ctx_t);
    char** dst_argv = (char**)p;
    p += argv_size;
    char* dst_arg_strings = p;
    p += arg_strings_size;
    char** dst_envv = (char**)p;
    p += envv_size;
    char* dst_env_strings = p;
    char* dst_str;


    // Argv
    dst_str = dst_arg_strings;
    for (size_t i = 0; i < argc; i++) {
        dst_argv[i] = dst_str;
        dst_str = strcpy_x(dst_str, argv[i]) + 1;
    }
    *dst_str = '\0';
    dst_argv[argc] = NULL;


    // Envp
    dst_str = dst_env_strings;
    for (size_t i = 0; i < envc; i++) {
        dst_envv[i] = dst_str;
        dst_str = strcpy_x(dst_str, env[i]) + 1;
    }
    *dst_str = '\0';
    dst_envv[envc] = NULL;


    // Process Arguments
    pctx->version = sizeof(proc_ctx_t);
    pctx->ctx_size = ctx_size;
    pctx->arg_size = arg_strings_size;
    pctx->arg_strings = dst_arg_strings;
    pctx->env_size = env_strings_size;
    pctx->env_strings = dst_env_strings;
    pctx->argc = argc;
    pctx->argv = dst_argv;
    pctx->envc = envc;
    pctx->envv = dst_envv;
    pctx->image_base = NULL;
    pctx->kei_funcs = gKeiTable;

    pimg->ctx_base = pctx;

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
    ac.group_id = VCPUID_MAIN_GROUP;
    ac.policy.version = sizeof(vcpu_policy_t);
    ac.policy.qos.grade = VCPU_QOS_INTERACTIVE;
    ac.policy.qos.priority = VCPU_PRI_NORMAL;
    ac.isUser = true;

    err = vcpu_acquire(&ac, &vp);

    *pOutVcpu = vp;
    return err;
}

// Loads an executable from the given executable file.
// \param self the process into which the executable image should be loaded
// \param path path to the executable file
// \param argv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param env the environment for the process. Null means that the process inherits the environment from its parent
static errno_t _proc_img_load_executable(proc_img_t* _Nonnull pimg, FileManagerRef _Nonnull fm, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    IOChannelRef chan = NULL;
    static const char* null_sptr[1] = {NULL};

    if (argv == NULL) {
        argv = null_sptr;
    }
    if (env == NULL) {
        env = null_sptr;
    }


    // Open the executable file and lock it
    try(FileManager_OpenFile(fm, path, O_RDONLY | _O_EXONLY, &chan));


    // Copy the process arguments into the process address space
    try(_proc_img_copy_args_env(pimg, argv, env));


    // Load the executable
    try(_proc_img_load_gemdos_file(pimg, chan));
    pimg->ctx_base->image_base = pimg->base;


catch:
    IOChannel_Release(chan);

    return err;
}

// Initializes a new process image by loading the executable file and building
// up the initial memory map.
static errno_t _proc_img_init(ProcessRef _Nonnull _Locked self, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[], proc_img_t* _Nonnull pimg)
{
    decl_try_err();

    AddressSpace_Init(&pimg->as);
    pimg->has_as = true;

    // Load the executable
    try(_proc_img_load_executable(pimg, &self->fm, path, argv, env));


    // Create the new main vcpu
    try(_proc_img_acquire_main_vcpu((vcpu_func_t)pimg->entry_point, pimg->ctx_base, &pimg->main_vp));


catch:

    return err;
}

static void _proc_img_deinit(proc_img_t* _Nullable pimg)
{
    if (pimg) {
        if (pimg->has_as) {
            AddressSpace_Deinit(&pimg->as);
        }
    }
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
    self->vcpu_lifetime_count++;
    pimg->main_vp->proc = self;
    self->ctx_base = pimg->ctx_base;
}

errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[], bool resumed)
{
    decl_try_err();
    proc_img_t pimg = (proc_img_t){0};

    mtx_lock(&self->mtx);

    // We only permit calling Process_Exit() from another process if that other
    // process is building us (thus there's no vcpu assigned to 'self' at this
    // point).
    assert(deque_empty(&self->vcpu_queue)
        || (!deque_empty(&self->vcpu_queue) && vcpu_current()->proc == self));

    
    // Don't do an exec() if we are in the process of being shut down
    if (vcpu_is_aborting(vcpu_current())) {
        throw(EINTR);
    }


    // Create the new exec image
    try(_proc_img_init(self, execPath, argv, env, &pimg));

    
    // We now got:
    // - a new address space with the executable image mapped in
    // - a new vcpu suitable to act as a main vcpu
    // we'll now demolish the existing executable image and install the new
    // address map and main vcpu
    _proc_img_deactivate_current(self);
    _proc_img_activate(self, &pimg);


catch:
    mtx_unlock(&self->mtx);

    if (resumed) {
        vcpu_resume(pimg.main_vp, false);
    }

    _proc_img_deinit(&pimg);

    return err;
}

void Process_ResumeMainVirtualProcessor(ProcessRef _Nonnull self)
{
    vcpu_resume(vcpu_from_owner_qe(self->vcpu_queue.first), false);
}