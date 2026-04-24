//
//  Process_Exec.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "proc_img.h"
#include <assert.h>
#include <ext/string.h>
#include <kei/kei.h>
#include <kern/kalloc.h>
#include <process/ProcessManager.h>
#include <sched/vcpu.h>


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
    try(proc_img_create(&self->fm, execPath, argv, env, &pimg));


    // Load the executable
    try(proc_img_load(pimg));


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

errno_t Process_SpawnChild(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const proc_spawn_t* _Nonnull opts, FileHierarchyRef _Nullable ovrFh, pid_t* _Nullable pOutPid)
{
    decl_try_err();
    ProcessRef cp = NULL;

    if (*path == '\0') {
        return EINVAL;
    }
    
    // Create the child process
    mtx_lock(&self->mtx);
    if (!vcpu_is_aborting(vcpu_current())) {
        err = proc_create_child(self, opts, ovrFh, &cp);
    }
    else {
        err = EINTR;
    }
    mtx_unlock(&self->mtx);
    throw_iferr(err);


    // Prepare the executable image
    try(Process_Exec(cp, path, argv, opts->envp, false));


    // Register the new process with the process manager
    try(ProcessManager_Publish(gProcessManager, cp));
    Process_Release(cp);


    // Start the child process running
    Process_ResumeMainVirtualProcessor(cp);

catch:
    if (err == EOK) {
        if (pOutPid) {
            *pOutPid = cp->pid;
        }
    }
    else {
        Process_Release(cp);
        if (pOutPid) {
            *pOutPid = 0;
        }
    }

    return err;
}
