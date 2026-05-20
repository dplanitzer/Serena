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


errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[])
{
    decl_try_err();
    proc_img_t* pimg = NULL;
    vcpu_t vcpu_to_resume = NULL;

    if (*execPath == '\0') {
        return EINVAL;
    }

    mtx_lock(&self->mtx);

    // We only permit calling Process_Terminate() from another process if that other
    // process is building us (thus there's no vcpu assigned to 'self' at this
    // point).
    assert(deque_empty(&self->vcpu_queue)
        || (!deque_empty(&self->vcpu_queue) && vcpu_current()->proc == self));


    if (_proc_is_terminating(self)) {
        throw(ECANCELED);
    }

    
    // Create the new exec image
    try(proc_img_create(&self->fm, execPath, argv, env, &pimg));


    // Load the executable
    try(proc_img_load(pimg));


    if (self->ctx_base == NULL) {
        // No executable image exists at this point and thus this is the first
        // time we're doing a proc_exec() in this process. Acquire a new main
        // vcpu from scratch.
        assert(vcpu_current()->proc != self);
        _vcpu_acquire_attr_t ac;

        ac.func = (VoidFunc_1)pimg->entry_point;
        ac.arg = pimg->ctx_base;
        ac.stack_size = PROC_DEFAULT_USER_STACK_SIZE;
        ac.group_id = VCPUID_MAIN_GROUP;
        ac.policy.version = sizeof(vcpu_policy_t);
        ac.policy.qos.grade = VCPU_QOS_INTERACTIVE;
        ac.policy.qos.priority = VCPU_PRI_NORMAL;
        ac.flags = 0;
        ac.data = 0;

        try(_proc_acquire_vcpu(self, &ac, true, &vcpu_to_resume));
    }
    else {
        // We're replacing an existing executable image. The current vcpu will
        // become the new main vcpu.
        vcpu_t me_vp = vcpu_current();

        assert(me_vp->proc == self);

        // Destroy the existing process image
        _proc_abort_other_vcpus(self);

        mtx_unlock(&self->mtx);
        _proc_reap_vcpus(self);
        mtx_lock(&self->mtx);

        _proc_destroy_sigroutes_except_for_vcpuid(self, me_vp->id);
        IOChannelTable_ReleaseChannelsOnExec(&self->ioChannelTable);


        // Reset our user stack so that we'll start executing the new process
        // image once we return to user space
        _vcpu_reset_user_stack(me_vp, (VoidFunc_1)pimg->entry_point, pimg->ctx_base, (VoidFunc_0)vcpu_uret_exit);
        

        // Rename this vcpu so that it will be known as the main vcpu 
        _proc_reassign_sigroutes_to_vcpuid(self, me_vp->id, VCPUID_MAIN);
        me_vp->id = VCPUID_MAIN;
        me_vp->group_id = VCPUID_MAIN_GROUP;
    }


    // Free the old address space and switch to the new one 
    AddressSpace_AdoptMappingsFrom(&self->addr_space, &pimg->as);
    self->ctx_base = pimg->ctx_base;
    self->arg_size = pimg->arg_size;
    self->arg_strings = pimg->arg_strings;
    self->env_size = pimg->env_size;
    self->env_strings = pimg->env_strings;


catch:
    proc_img_destroy(pimg);

    if (err == EOK && self->run_state == PROC_STATE_RUNNING && vcpu_to_resume) {
        vcpu_resume(vcpu_to_resume, false);
    }

    mtx_unlock(&self->mtx);

    return err;
}
