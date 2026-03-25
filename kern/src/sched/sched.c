//
//  sched.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "sched.h"
#include "vcpu.h"
#include "waitqueue.h"
#include <assert.h>
#include <string.h>
#include <ext/bit.h>
#include <hal/sched.h>
#include <kern/kernlib.h>
#include <kern/signal.h>


static void sched_dump_rdyq_locked(sched_t _Nonnull self);

static vcpu_t _Nonnull boot_vcpu_create(BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak arg);
static vcpu_t _Nonnull idle_vcpu_create(BootAllocator* _Nonnull bap);


sched_t                 g_sched;
static struct waitqueue g_sched_wq;     // The scheduler VP waits on this queue

// Note that a quantum tick is 16.667ms
const int8_t g_quantum_base_length[SCHED_QOS_COUNT] = {
    SCHED_QUANTUM(1),       /* Realtime */
    SCHED_QUANTUM(2),       /* Urgent */
    SCHED_QUANTUM(4),       /* Interactive */
    SCHED_QUANTUM(8),       /* Utility */
    SCHED_QUANTUM(12)       /* Background */
};
const int8_t g_quantum_max_length[SCHED_QOS_COUNT] = {
    SCHED_QUANTUM(2),       /* Realtime */
    SCHED_QUANTUM(3),       /* Urgent */
    SCHED_QUANTUM(6),       /* Interactive */
    SCHED_QUANTUM(10),      /* Utility */
    SCHED_QUANTUM(14)       /* Background */
};


void sched_create(BootAllocator* _Nonnull bap, sys_desc_t* _Nonnull sdp, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx)
{
    sched_t self = BootAllocator_Allocate(bap, sizeof(struct sched));
    memset(self, 0, sizeof(struct sched));
    g_sched = self;


    wq_init(&g_sched_wq);
    vcpu_platform_init();


    // Initialize the boot virtual processor
    self->boot_vp = boot_vcpu_create(bap, fn, ctx);
    sched_set_ready(self, self->boot_vp, true);


    // Initialize the idle virtual processor
    self->idle_vp = idle_vcpu_create(bap);
    sched_set_ready(self, self->idle_vp, true);


    // Initialize the scheduler    
    self->running = NULL;
    sched_set_running(self, sched_highest_priority_ready(self));
    
    assert(self->scheduled == self->boot_vp);
}

// Marks 'vp' as ready and inserts it in the proper ready queue.
// Called from: _sched_switch_context()
void sched_set_ready(sched_t _Nonnull self, vcpu_t _Nonnull vp, bool doAddToTail)
{
    assert(vp != NULL);
    assert(vp->rewa_qe.prev == NULL);
    assert(vp->rewa_qe.next == NULL);
    

    vp->sched_state = SCHED_STATE_READY;
    const unsigned int pri = vp->effective_priority;

    if (doAddToTail) {
        deque_add_last(&self->ready_queue.priority[pri], &vp->rewa_qe);
    }
    else {
        deque_add_first(&self->ready_queue.priority[pri], &vp->rewa_qe);
    }
    
    sched_rq_mark_populated(&self->ready_queue, pri);
}

// Takes the given virtual processor off the ready queue and marks it as running
// if requested. 'doReadyToRun' is true if the vp should be moved to running state
// in addition to moving it off the ready queue. If this is false, then the vp
// should be simply taken off the ready queue but its scheduler state should not
// be changed because it will moved back to the ready queue shortly (or enter
// suspended state shortly).
// Called from: _sched_switch_context()
void sched_set_unready(sched_t _Nonnull self, vcpu_t _Nonnull vp, bool doReadyToRun)
{
    const unsigned int pri = vp->effective_priority;
    
    if (doReadyToRun) {
        if (!stk_isvalidsp(&vp->kernel_stack, vp->csw_sa)) {
            abort();
        }
        if (vcpu_isuser(vp) && !stk_isvalidsp(&vp->user_stack, vp->csw_sa->usp)) {
            abort();
        }

        vp->sched_state = SCHED_STATE_RUNNING;
    }


    deque_remove(&self->ready_queue.priority[pri], &vp->rewa_qe);
    
    if (deque_empty(&self->ready_queue.priority[pri])) {
        sched_rq_clear_populated(&self->ready_queue, pri);
    }
}

#if !defined(__M68K__)
//m68k: sched_csw.s
vcpu_t _Nullable sched_highest_priority_ready(sched_t _Nonnull self)
{
    unsigned int lzc;

#if (SCHED_POP_WORD_COUNT == 3 && SCHED_POP_WORD_WIDTH == 32)
    lzc = leading_zeros_ui(self->ready_queue.populated[2]);
    if (lzc < 32) {
        return (vcpu_t)self->ready_queue.priority[(31 - lzc) + 64].first;
    }

    lzc = leading_zeros_ui(self->ready_queue.populated[1]);
    if (lzc < 32) {
        return (vcpu_t)self->ready_queue.priority[(31 - lzc) + 32].first;
    }

    lzc = leading_zeros_ui(self->ready_queue.populated[0]);
    if (lzc < 32) {
        return (vcpu_t)self->ready_queue.priority[31 - lzc].first;
    }
    return NULL;

#elif (SCHED_POP_WORD_COUNT == 2 && SCHED_POP_WORD_WIDTH == 64)
    lzc = leading_zeros_ull(self->ready_queue.populated[1]);
    if (lzc < 64) {
        return (vcpu_t)self->ready_queue.priority[(63 - lzc) + 64].first;
    }

    // This always returns a value < 32 since the idle priority always
    // has a vcpu that's always ready to run
    lzc = leading_zeros_ull(self->ready_queue.populated[0]);
    if (lzc < 64) {
        return (vcpu_t)self->ready_queue.priority[63 - lzc].first;
    }
    return NULL;

#else
#error "unknown pop_word_t size"
#endif
}
#endif

// Context switch to the given virtual processor. 'vp' must be in ready state
// and on the ready queue. Immediately triggers a context switch to 'vp'.
// Expects that the outgoing virtual processor is either still in state running
// or that it has been transitioned to a waiting or suspended state.
// @Entry Condition: preemption disabled
void sched_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    sched_set_running(self, vp);
    sched_switch_context();
}

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
_Noreturn void sched_terminate_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    // NOTE: We don't need to save the old preemption state because this VP is
    // going away and we will never context switch back to it. The context switch
    // will reenable preemption.
    (void) preempt_disable();


    vp->sched_state = SCHED_STATE_TERMINATING;
    deque_add_last(&self->finalizer_queue, &vp->owner_qe);
    
    
    // Check whether there are too many VPs on the finalizer queue. If so then we
    // try to context switch to the scheduler VP otherwise we'll context switch
    // to whoever else is the best candidate to run.
    vcpu_t newRunning;
    const int FINALIZE_NOW_THRESHOLD = 4;
    int dead_vps_count = 0;
    deque_node_t* pCurNode = self->finalizer_queue.first;
    while (pCurNode != NULL && dead_vps_count < FINALIZE_NOW_THRESHOLD) {
        pCurNode = pCurNode->next;
        dead_vps_count++;
    }
    
    if (dead_vps_count >= FINALIZE_NOW_THRESHOLD && g_sched_wq.q.first != NULL) {
        // The scheduler VP is currently waiting for work. Let's wake it up.
        wq_wakeup_vcpu(&g_sched_wq, self->boot_vp, 0, WRES_WAKEUP, 0);
    } else {
        // Do a forced context switch to whoever is ready
        // NOTE: we do NOT put the currently running VP back on the ready queue
        // because it is dead.
        sched_switch_to(self, sched_highest_priority_ready(self));
    }
    
    /* NOT REACHED */
}

// Gives the virtual processor scheduler opportunities to run tasks that take
// care of internal duties. This function must be called from the boot virtual
// processor. This function does not return to the caller. 
_Noreturn void sched_run_chores(sched_t _Nonnull self)
{
    deque_t dead_vps;
    struct timespec now, timeout, deadline;

    timespec_from_sec(&timeout, 1);
    
    while (true) {
        dead_vps = DEQUE_INIT;
        const int sps = preempt_disable();

        // Continue to wait as long as there's nothing to finalize
        while (deque_empty(&self->finalizer_queue)) {
            (void)wq_timedwait(&g_sched_wq,
                                &SIGSET_IGNORE_ALL,
                                0,
                                &timeout,
                                NULL);
        }
        
        // Got some work to do. Save off the needed data in local vars and then
        // reenable preemption before we go and do the actual work.
        dead_vps = self->finalizer_queue;
        self->finalizer_queue = DEQUE_INIT;
        
        preempt_restore(sps);
        
        
        // Finalize VPs which have exited
        deque_for_each(&dead_vps, deque_node_t, it,
            vcpu_t cp = vcpu_from_owner_qe(it);

            vcpu_destroy(cp);
        )
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Boot Virtual Processor
////////////////////////////////////////////////////////////////////////////////


// Initializes a boot virtual processor. This is the virtual processor which
// is used to grandfather in the initial thread of execution at boot time. It is
// the first VP that is created for a physical processor. It then takes over
// duties for the scheduler.
// \param pVP the boot virtual processor record
// \param closure the closure that should be invoked by the virtual processor
static vcpu_t _Nonnull boot_vcpu_create(BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak arg)
{
    decl_try_err();
    vcpu_t self = BootAllocator_Allocate(bap, sizeof(struct vcpu));
    memset(self, 0, sizeof(struct vcpu));


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = min_vcpu_kernel_stack_size();
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    vcpu_policy_t policy;
    policy.qos_class = SCHED_QOS_REALTIME;
    policy.qos_priority = QOS_PRI_HIGHEST;
    vcpu_init(self, &policy);

    vcpu_acquisition_t ac = VCPU_ACQUISITION_INIT;
    ac.func = (VoidFunc_1)fn;
    ac.arg = arg;
    ac.kernelStackBase = pKernelStackBase;
    ac.kernelStackSize = kernelStackSize;
    ac.policy = policy;
    ac.isUser = false;

    try_bang(_vcpu_reset_mcontext(self, &ac, false));
    
    return self;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Idle Virtual Processor
////////////////////////////////////////////////////////////////////////////////

static void idle_vcpu_run(void* _Nullable ctx);

// Creates an idle virtual processor. The scheduler schedules this VP if no other
// one is in state ready.
static vcpu_t _Nonnull idle_vcpu_create(BootAllocator* _Nonnull bap)
{
    decl_try_err();
    vcpu_t self = BootAllocator_Allocate(bap, sizeof(struct vcpu));
    memset(self, 0, sizeof(struct vcpu));


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = min_vcpu_kernel_stack_size();
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    vcpu_policy_t policy;
    policy.qos_class = SCHED_QOS_BACKGROUND;
    policy.qos_priority = QOS_PRI_LOWEST;
    vcpu_init(self, &policy);

    vcpu_acquisition_t ac = VCPU_ACQUISITION_INIT;
    ac.func = (VoidFunc_1)idle_vcpu_run;
    ac.kernelStackBase = pKernelStackBase;
    ac.kernelStackSize = kernelStackSize;
    ac.policy = policy;
    ac.isUser = false;

    try_bang(_vcpu_reset_mcontext(self, &ac, true));

    return self;
}

// Puts the CPU to sleep until an interrupt occurs. The interrupt will give the
// scheduler a chance to run some other virtual processor if one is ready.
static void idle_vcpu_run(void* _Nullable ctx)
{
    while (true) {
        cpu_sleep(g_sys_desc->cpu_model);
    }
}
