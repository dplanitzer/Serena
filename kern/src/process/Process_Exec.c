//
//  Process_Exec.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ldr_gemdos.h"
#include <assert.h>
#include <ext/string.h>
#include <kei/kei.h>
#include <kern/kalloc.h>
#include <sched/vcpu.h>

typedef struct ctx_table {
    ssize_t                     tb_strings_size;    // Size of arg_strings in terms of bytes. Includes the trailing '\0'
    char* _Nonnull              tb_strings;         // Consecutive list of NUL-terminated process argument strings. End is marked by an empty string  
    size_t                      tbc;                // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  tbv;                // Pointer to the base of the command line arguments table. Last entry is NULL
} ctx_table_t;



// Calculates the size of the string array that will be used to store the 'argv'
// strings. This includes the NUL marking the end of each string and the final
// NUL entry that marks the end of the string array. Note this is not the argv[]
// vector that points to the string. This is a separate thing.
static errno_t _get_table_size(const char* const _Nullable tb[], ctx_table_t* _Nonnull ctb)
{
    ssize_t nbytes = 0;
    size_t count = 0;

    while (tb[count]) {
        const char* p = tb[count];
        const ssize_t slen = strnlen_s(p, __ARG_STRLEN_MAX);
        const ssize_t entry_size = slen + 1;

        if (p[slen] != '\0' || (nbytes + entry_size) > __ARG_MAX) {
            return E2BIG;
        }

        nbytes += entry_size;
        count++;
    }
    ctb->tbc = count;
    ctb->tb_strings_size = nbytes + 1;   // +1 is for the empty string ('\0') that marks the end of the string array

    return EOK;
}

static void _copyin_table(const char* tb[], const ctx_table_t* _Nonnull ctb)
{
    char** dst_tbv = ctb->tbv;
    char* p = ctb->tb_strings;

    for (size_t i = 0; i < ctb->tbc; i++) {
        dst_tbv[i] = p;
        p = strcpy_x(p, tb[i]) + 1;
    }
    *p = '\0';
    dst_tbv[ctb->tbc] = NULL;
}

static void _copyin_aux_array(proc_aux_entry_t* _Nonnull aux, void* _Nonnull exec_hdr)
{
    aux[0].type = PROC_AUX_EXEC_HDR;
    aux[0].u.p = exec_hdr;

    aux[1].type = PROC_AUX_KEI;
    aux[1].u.p = gKeiTable;
    
    aux[2].type = PROC_AUX_END;
    aux[2].u.p = NULL;
}

static errno_t proc_img_build_ctx(proc_img_t* _Nonnull pimg, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    static const char* null_sptr[1] = {NULL};

    if (argv == NULL) {
        argv = null_sptr;
    }
    if (env == NULL) {
        env = null_sptr;
    }


    // Build the proc_ctx_t region. Data is laid out like this to ensure proper
    // alignment:
    // proc_ctx_t, argv_table, envv_table, aux_array, arg_strings, env_strings
    //
    // Layout of the aux entries:
    // exec_hdr, kei_ptr, aux_end
    ctx_table_t dst_argv;
    ctx_table_t dst_env;
    proc_aux_entry_t* aux_array;

    try(_get_table_size(argv, &dst_argv));
    try(_get_table_size(env, &dst_env));

#define AUX_ENTRY_COUNT 3
    proc_ctx_t* pctx = NULL;
    const size_t argv_size = sizeof(char*) * (dst_argv.tbc + 1);
    const size_t envv_size = sizeof(char*) * (dst_env.tbc + 1);
    const size_t aux_size = sizeof(proc_aux_entry_t) * AUX_ENTRY_COUNT;
    const size_t pure_ctx_rgn_size = sizeof(proc_ctx_t) + argv_size + envv_size + aux_size + dst_argv.tb_strings_size + dst_env.tb_strings_size;
    const ssize_t ctx_rgn_size = __Ceil_PowerOf2(pure_ctx_rgn_size, CPU_PAGE_SIZE);

    try(AddressSpace_Allocate(&pimg->as, ctx_rgn_size, (void**)&pctx));

    dst_argv.tbv = (char**)((char*)pctx + sizeof(proc_ctx_t));
    dst_env.tbv = (char**)((char*)dst_argv.tbv + argv_size);
    aux_array = (proc_aux_entry_t*)((char*)dst_env.tbv + envv_size);
    dst_argv.tb_strings = (char*)aux_array + aux_size;
    dst_env.tb_strings = dst_argv.tb_strings + dst_argv.tb_strings_size;

    _copyin_table(argv, &dst_argv);
    _copyin_table(env, &dst_env);
    _copyin_aux_array(aux_array, pimg->base);

    pctx->argc = dst_argv.tbc;
    pctx->argv = dst_argv.tbv;
    pctx->envc = dst_env.tbc;
    pctx->envv = dst_env.tbv;
    pctx->aux = aux_array;

    pimg->ctx_base = pctx;
    pimg->arg_size = dst_argv.tb_strings_size;
    pimg->arg_strings = dst_argv.tb_strings;
    pimg->env_size = dst_env.tb_strings_size;
    pimg->env_strings = dst_env.tb_strings;

catch:
    return err;
}

// Loads an executable from the given executable file.
// \param self the process into which the executable image should be loaded
// \param path path to the executable file
// \param argv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param env the environment for the process. Null means that the process inherits the environment from its parent
static errno_t proc_img_load(proc_img_t* _Nonnull pimg, FileManagerRef _Nonnull fm, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    IOChannelRef chan = NULL;


    // Open the executable file and lock it
    try(FileManager_OpenFile(fm, path, O_RDONLY | _O_EXONLY, &chan));


    // Load the executable
    try(ldr_gemdos_load(pimg, chan));


    // Create the proc context
    try(proc_img_build_ctx(pimg, argv, env));


catch:
    IOChannel_Release(chan);

    return err;
}

// Initializes a new process image by loading the executable file and building
// up the initial memory map.
static errno_t proc_img_create(ProcessRef _Nonnull _Locked self, const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable env[], proc_img_t* _Nullable * _Nonnull pOutImg)
{
    decl_try_err();
    proc_img_t* pimg;

    try(kalloc_cleared(sizeof(struct proc_img), (void**)&pimg));
    AddressSpace_Init(&pimg->as);
    
catch:
    *pOutImg = pimg;

    return err;
}

static void proc_img_destroy(proc_img_t* _Nullable pimg)
{
    if (pimg) {
        AddressSpace_Deinit(&pimg->as);
        kfree(pimg);
    }
}


static errno_t _acquire_main_vcpu(vcpu_func_t _Nonnull entryPoint, void* _Nonnull procargs, vcpu_t _Nonnull * _Nullable pOutVcpu)
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

static void _proc_destroy_pimg(ProcessRef _Nonnull self)
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

static void _proc_install_pimg(ProcessRef _Nonnull self, const proc_img_t* _Nonnull pimg, vcpu_t new_main_vcpu)
{
    AddressSpace_AdoptMappingsFrom(&self->addr_space, &pimg->as);

    new_main_vcpu->proc = self;
    deque_add_last(&self->vcpu_queue, &new_main_vcpu->owner_qe);
    self->vcpu_count++;
    self->vcpu_lifetime_count++;

    self->ctx_base = pimg->ctx_base;
    self->arg_size = pimg->arg_size;
    self->arg_strings = pimg->arg_strings;
    self->env_size = pimg->env_size;
    self->env_strings = pimg->env_strings;
}

errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[], bool resumed)
{
    decl_try_err();
    proc_img_t* pimg = NULL;
    vcpu_t new_main_vcpu = NULL;

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
    try(proc_img_create(self, execPath, argv, env, &pimg));


    // Load the executable
    try(proc_img_load(pimg, &self->fm, execPath, argv, env));


    // Create the new main vcpu
    try(_acquire_main_vcpu((vcpu_func_t)pimg->entry_point, pimg->ctx_base, &new_main_vcpu));

    
    // We now got:
    // - a new address space with the executable image mapped in
    // - a new vcpu suitable to act as a main vcpu
    // we'll now demolish the existing executable image and install the new
    // address map and main vcpu
    _proc_destroy_pimg(self);
    _proc_install_pimg(self, pimg, new_main_vcpu);


catch:
    mtx_unlock(&self->mtx);

    proc_img_destroy(pimg);

    if (err == EOK && new_main_vcpu && resumed) {
        vcpu_resume(new_main_vcpu, false);
    }

    return err;
}

void Process_ResumeMainVirtualProcessor(ProcessRef _Nonnull self)
{
    vcpu_resume(vcpu_from_owner_qe(self->vcpu_queue.first), false);
}