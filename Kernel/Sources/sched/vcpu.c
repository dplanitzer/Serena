//
//  vcpu.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu.h"
#include "sched.h"
#include "vcpu_pool.h"
#include <machine/clock.h>
#include <machine/csw.h>
#include <kern/assert.h>
#include <kern/kalloc.h>
#include <kern/limits.h>
#include <kern/string.h>
#include <kern/timespec.h>


// Frees a virtual processor.
// \param pVP the virtual processor
void __func_vcpu_destroy(vcpu_t _Nullable self)
{
    stk_destroy(&self->kernel_stack);
    stk_destroy(&self->user_stack);
    kfree(self);
}

static struct vcpu_vtable gVirtualProcessorVTable = {
    __func_vcpu_destroy
};


// Relinquishes the virtual processor which means that it is finished executing
// code and that it should be moved back to the virtual processor pool. This
// function does not return to the caller. This function should only be invoked
// from the bottom-most frame on the virtual processor's kernel stack.
_Noreturn vcpu_relinquish(void)
{
    vcpu_pool_relinquish(g_vcpu_pool, vcpu_current());
    /* NOT REACHED */
}

// Initializes a virtual processor. A virtual processor always starts execution
// in supervisor mode. The user stack size may be 0. Note that a virtual processor
// always starts out in suspended state.
//
// \param pVP the boot virtual processor record
// \param priority the initial VP priority
void vcpu_cominit(vcpu_t _Nonnull self, const sched_params_t* _Nonnull sched_params, bool suspended)
{
    assert(sched_params->type == SCHED_PARAM_QOS);

    self->rewa_qe = LISTNODE_INIT;
    stk_init(&self->kernel_stack);
    stk_init(&self->user_stack);

    self->vtable = &gVirtualProcessorVTable;
    
    self->owner_qe = LISTNODE_INIT;
    self->timeout = CLOCK_DEADLINE_INIT;
    
    self->pending_sigs = 0;
    self->proc_sigs_enabled = 0;

    self->excpt_handler = (excpt_handler_t){0};
    
    self->waiting_on_wait_queue = NULL;
    self->wait_sigs = 0;
    self->wakeup_reason = 0;
    
    self->sched_state = SCHED_STATE_READY;
    self->flags = (g_sys_desc->fpu_model > FPU_MODEL_NONE) ? VP_FLAG_HAS_FPU : 0;
    self->qos = sched_params->u.qos.category;
    self->qos_priority = sched_params->u.qos.priority;
    self->priority_bias = 0;
    self->suspension_count = (suspended) ? 1 : 0;
    
    self->id = 0;
    self->lifecycle_state = VP_LIFECYCLE_RELINQUISHED;

    self->dispatchQueue = NULL;
    self->dispatchQueueConcurrencyLaneIndex = -1;

    vcpu_sched_params_changed(self);
}

// Creates a new virtual processor.
// \return the new virtual processor; NULL if creation has failed
errno_t vcpu_create(const sched_params_t* _Nonnull sched_params, vcpu_t _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    vcpu_t self = NULL;
    
    err = kalloc_cleared(sizeof(struct vcpu), (void**) &self);
    if (err == EOK) {
        vcpu_cominit(self, sched_params, true);
    }

    *pOutSelf = self;
    return err;
}

void vcpu_destroy(vcpu_t _Nullable self)
{
    if (self) {
        self->vtable->destroy(self);
    }
}

// Sets the dispatch queue that has acquired the virtual processor and owns it
// until the virtual processor is relinquished back to the virtual processor
// pool.
void vcpu_setdq(vcpu_t _Nonnull self, void* _Nullable pQueue, int concurrencyLaneIndex)
{
    VP_ASSERT_ALIVE(self);
    self->dispatchQueue = pQueue;
    self->dispatchQueueConcurrencyLaneIndex = concurrencyLaneIndex;
}

// Terminates the virtual processor that is executing the caller. Does not return
// to the caller. Note that the actual termination of the virtual processor is
// handled by the virtual processor scheduler.
_Noreturn vcpu_terminate(vcpu_t _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    self->lifecycle_state = VP_LIFECYCLE_TERMINATING;

    sched_terminate_vcpu(g_sched, self);
    /* NOT REACHED */
}

void vcpu_reduce_sched_penalty(vcpu_t _Nonnull self, int weight)
{
    if (self->priority_bias < 0) {
        const int bias = self->priority_bias + weight * 2;

        if (bias < 0) {
            self->priority_bias = bias;
        }
        else {
            self->priority_bias = 0;
        }

        vcpu_sched_params_changed(self);
    }
}

void vcpu_sched_params_changed(vcpu_t _Nonnull self)
{
    int sched_pri, eff_pri;

    if (self->qos > SCHED_QOS_IDLE) {
        sched_pri = ((self->qos - 1) << QOS_PRI_SHIFT) + (self->qos_priority - QOS_PRI_LOWEST) + 1;
        eff_pri = sched_pri + self->priority_bias;
        eff_pri = __max(__min(eff_pri, SCHED_PRI_HIGHEST), SCHED_PRI_LOWEST + 1);
    }
    else {
        // SCHED_QOS_IDLE has only one priority level
        eff_pri = SCHED_PRI_LOWEST;
    }

    assert(eff_pri >= SCHED_PRI_LOWEST && eff_pri <= SCHED_PRI_HIGHEST);

    self->sched_priority = (uint8_t)sched_pri;
    self->effective_priority = (uint8_t)eff_pri;
}

errno_t vcpu_getschedparams(vcpu_t _Nonnull self, int type, sched_params_t* _Nonnull params)
{
    decl_try_err();
    const int sps = preempt_disable();

    switch (type) {
        case SCHED_PARAM_QOS:
            params->type = SCHED_PARAM_QOS;
            params->u.qos.category = self->qos;
            params->u.qos.priority = self->qos_priority;
            break;

        default:
            err = EINVAL;
            break;
    }
    preempt_restore(sps);

    return err;
}

errno_t vcpu_setschedparams(vcpu_t _Nonnull self, const sched_params_t* _Nonnull params)
{
    VP_ASSERT_ALIVE(self);

    if (params->type != SCHED_PARAM_QOS) {
        return EINVAL;
    }
    if (params->u.qos.category < SCHED_QOS_BACKGROUND || params->u.qos.category > SCHED_QOS_REALTIME) {
        return EINVAL;
    }
    if (params->u.qos.priority < QOS_PRI_LOWEST || params->u.qos.priority > QOS_PRI_HIGHEST) {
        return EINVAL;
    }


    const int sps = preempt_disable();
    
    if (self->qos != params->u.qos.category || self->qos_priority != params->u.qos.priority) {
        switch (self->sched_state) {
            case SCHED_STATE_READY:
                if (self->suspension_count == 0) {
                    sched_set_unready(g_sched, self);
                }
                self->qos = params->u.qos.category;
                self->qos_priority = params->u.qos.priority;
                vcpu_sched_params_changed(self);
                if (self->suspension_count == 0) {
                    sched_set_ready(g_sched, self, true);
                }
                break;
                
            case SCHED_STATE_WAITING:
                self->qos = params->u.qos.category;
                self->qos_priority = params->u.qos.priority;
                vcpu_sched_params_changed(self);
                break;
                
            case SCHED_STATE_RUNNING:
                self->qos = params->u.qos.category;
                self->qos_priority = params->u.qos.priority;
                self->quantum_countdown = qos_quantum(self->qos);
                vcpu_sched_params_changed(self);
                break;
        }
    }
    preempt_restore(sps);
}

int vcpu_getcurrentpriority(vcpu_t _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    const uint8_t pri = self->effective_priority;
    
    preempt_restore(sps);
    return (int)pri;
}

// Yields the remainder of the current quantum to other VPs.
void vcpu_yield(void)
{
    const int sps = preempt_disable();
    vcpu_t self = (vcpu_t)g_sched->running;

    assert(self->sched_state == SCHED_STATE_RUNNING && self->suspension_count == 0);

    vcpu_reduce_sched_penalty(self, 1);
    sched_switch_to(g_sched, sched_highest_priority_ready(g_sched), true);    
    preempt_restore(sps);
}

// Suspends the calling virtual processor. This function supports nested calls.
errno_t vcpu_suspend(vcpu_t _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    
    if (self->suspension_count == INT8_MAX) {
        preempt_restore(sps);
        return EINVAL;
    }
    
    self->suspension_count++;
    
    if (self->suspension_count == 1) {
        self->suspension_time = clock_getticks(g_mono_clock);

        switch (self->sched_state) {
            case SCHED_STATE_READY:
                sched_set_unready(g_sched, self);
                break;
            
            case SCHED_STATE_RUNNING:
                // We're running, thus we are not on the ready queue. Do a forced
                // context switch to some other VP.
                sched_switch_to(g_sched, sched_highest_priority_ready(g_sched), false);
                break;
            
            case SCHED_STATE_WAITING:
                wq_suspendone(self->waiting_on_wait_queue, self);
                break;
            
            default:
                abort();
        }
    }
    
    preempt_restore(sps);
    return EOK;
}

// Resumes the given virtual processor. The virtual processor is forcefully
// resumed if 'force' is true. This means that it is resumed even if the suspension
// count is > 1.
void vcpu_resume(vcpu_t _Nonnull self, bool force)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    
    if (self->suspension_count > 0) {
        if (force) {
            self->suspension_count = 0;
        }
        else {
            self->suspension_count--;
        }


        if (self->suspension_count == 0) {
            switch (self->sched_state) {
                case SCHED_STATE_READY:
                    vcpu_reduce_sched_penalty(self, 1);
                    sched_set_ready(g_sched, self, true);
                    break;

                case SCHED_STATE_WAITING:
                    wq_resumeone(self->waiting_on_wait_queue, self);
                    break;
            
                default:
                    abort();
            }
        }
    }
    preempt_restore(sps);
}

// Returns true if the given virtual processor is currently suspended; false otherwise.
bool vcpu_suspended(vcpu_t _Nonnull self)
{
    VP_ASSERT_ALIVE(self);
    const int sps = preempt_disable();
    const bool isSuspended = self->suspension_count > 0;
    preempt_restore(sps);
    return isSuspended;
}

vcpuid_t new_vcpu_groupid(void)
{
    static vcpuid_t id = VCPUID_MAIN_GROUP;

    return AtomicInt_Increment((volatile AtomicInt*)&id);
}
