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
#include <machine/clock.h>
#include <machine/csw.h>
#include <kern/string.h>
#include <kern/timespec.h>


static void sched_dump_rdyq_locked(sched_t _Nonnull self);

static vcpu_t _Nonnull boot_vcpu_create(BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx);
static vcpu_t _Nonnull idle_vcpu_create(BootAllocator* _Nonnull bap);


sched_t                 g_sched;
static struct waitqueue g_sched_wq;     // The scheduler VP waits on this queue


void sched_create(BootAllocator* _Nonnull bap, sys_desc_t* _Nonnull sdp, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx)
{
    sched_t self = BootAllocator_Allocate(bap, sizeof(struct sched));
    memset(self, 0, sizeof(struct sched));
    g_sched = self;


    // Initialize the boot virtual processor
    self->boot_vp = boot_vcpu_create(bap, fn, ctx);


    // Initialize the idle virtual processor
    self->idle_vp = idle_vcpu_create(bap);


    // Initialize the scheduler
    wq_init(&g_sched_wq);
    sched_add_vcpu_locked(
        self,
        self->boot_vp,
        self->boot_vp->sched_priority);
    
    self->running = NULL;
    self->scheduled = sched_highest_priority_ready(self);
    self->csw_signals |= CSW_SIGNAL_SWITCH;
    sched_remove_vcpu_locked(self, self->scheduled);
    
    assert(self->scheduled == self->boot_vp);
}

void sched_finish_boot(sched_t _Nonnull self)
{
    decl_try_err();
    struct timespec ts;
    
    timespec_from_ms(&ts, 250);
    self->ticks_per_quarter_second = clock_time2ticks_ceil(g_mono_clock, &ts);


    // Resume the idle virtual processor
    vcpu_resume(self->idle_vp, false);
}

void sched_add_vcpu_locked(sched_t _Nonnull self, vcpu_t _Nonnull vp, int effectivePriority)
{
    assert(vp != NULL);
    assert(vp->rewa_qe.prev == NULL);
    assert(vp->rewa_qe.next == NULL);
    assert(vp->suspension_count == 0);
    
    vp->sched_state = SCHED_STATE_READY;
    vp->effectivePriority = effectivePriority;
    vp->ticks_allowance = QuantumDurationForPriority(vp->effectivePriority);
    vp->wait_start_time = clock_getticks(g_mono_clock);
    
    List_InsertAfterLast(&self->ready_queue.priority[vp->effectivePriority], &vp->rewa_qe);
    
    const int popByteIdx = vp->effectivePriority >> 3;
    const int popBitIdx = vp->effectivePriority - (popByteIdx << 3);
    self->ready_queue.populated[popByteIdx] |= (1 << popBitIdx);
}

// Adds the given virtual processor to the scheduler and makes it eligible for
// running.
void sched_add_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    // Protect against our scheduling code
    const int sps = preempt_disable();
    
    sched_add_vcpu_locked(self, vp, vp->sched_priority);
    preempt_restore(sps);
}

// Takes the given virtual processor off the ready queue.
void sched_remove_vcpu_locked(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    register const int pri = vp->effectivePriority;
    
    List_Remove(&self->ready_queue.priority[pri], &vp->rewa_qe);
    
    if (List_IsEmpty(&self->ready_queue.priority[pri])) {
        const int i = pri >> 3;
        const int popBitIdx = pri - (i << 3);

        self->ready_queue.populated[i] &= ~(1 << popBitIdx);
    }
}

// Find the best VP to run next and return it. Null is returned if no VP is ready
// to run. This will only happen if this function is called from the quantum
// interrupt while the idle VP is the running VP.
vcpu_t _Nullable sched_highest_priority_ready(sched_t _Nonnull self)
{
    for (int i = VP_PRIORITY_POP_BYTE_COUNT - 1; i >= 0; i--) {
        register const uint8_t pop = self->ready_queue.populated[i];
        
        if (pop) {
            if (pop & 0x80) { return (vcpu_t) self->ready_queue.priority[(i << 3) + 7].first; }
            if (pop & 0x40) { return (vcpu_t) self->ready_queue.priority[(i << 3) + 6].first; }
            if (pop & 0x20) { return (vcpu_t) self->ready_queue.priority[(i << 3) + 5].first; }
            if (pop & 0x10) { return (vcpu_t) self->ready_queue.priority[(i << 3) + 4].first; }
            if (pop & 0x8)  { return (vcpu_t) self->ready_queue.priority[(i << 3) + 3].first; }
            if (pop & 0x4)  { return (vcpu_t) self->ready_queue.priority[(i << 3) + 2].first; }
            if (pop & 0x2)  { return (vcpu_t) self->ready_queue.priority[(i << 3) + 1].first; }
            if (pop & 0x1)  { return (vcpu_t) self->ready_queue.priority[(i << 3) + 0].first; }
        }
    }
    
    return NULL;
}

// Context switches to the given virtual processor if it is a better choice. Eg
// it has a higher priority than the VP that is currently running. This is a
// voluntary (cooperative) context switch which means that it will only happen
// if we are not running in the interrupt context and voluntary context switches
// are enabled.
void sched_maybe_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    if (vp->sched_state == SCHED_STATE_READY
        && vp->suspension_count == 0) {
        vcpu_t pBestReadyVP = sched_highest_priority_ready(self);
        
        if (pBestReadyVP == vp && vp->effectivePriority >= self->running->effectivePriority) {
            vcpu_t pCurRunning = (vcpu_t)self->running;
            
            sched_add_vcpu_locked(self, pCurRunning, pCurRunning->sched_priority);
            sched_switch_to(self, vp);
        }
    }
}

// Context switch to the given virtual processor. The VP must be in ready state
// and on the ready queue. Immediately context switches to the VP.
// Expects that the call has already added the currently running VP to a wait
// queue or the finalizer queue.
void sched_switch_to(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    sched_remove_vcpu_locked(self, vp);
    self->scheduled = vp;
    self->csw_signals |= CSW_SIGNAL_SWITCH;
    csw_switch();
}

// Terminates the given virtual processor that is executing the caller. Does not
// return to the caller. The VP must already have been marked as terminating.
_Noreturn sched_terminate_vcpu(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    // NOTE: We don't need to save the old preemption state because this VP is
    // going away and we will never context switch back to it. The context switch
    // will reenable preemption.
    (void) preempt_disable();


    List_InsertAfterLast(&self->finalizer_queue, &vp->owner_qe);
    
    
    // Check whether there are too many VPs on the finalizer queue. If so then we
    // try to context switch to the scheduler VP otherwise we'll context switch
    // to whoever else is the best candidate to run.
    vcpu_t newRunning;
    const int FINALIZE_NOW_THRESHOLD = 4;
    int dead_vps_count = 0;
    ListNode* pCurNode = self->finalizer_queue.first;
    while (pCurNode != NULL && dead_vps_count < FINALIZE_NOW_THRESHOLD) {
        pCurNode = pCurNode->next;
        dead_vps_count++;
    }
    
    if (dead_vps_count >= FINALIZE_NOW_THRESHOLD && g_sched_wq.q.first != NULL) {
        // The scheduler VP is currently waiting for work. Let's wake it up.
        wq_wakeone(&g_sched_wq, self->boot_vp, WAKEUP_CSW, WRES_WAKEUP);
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
_Noreturn sched_run_chores(sched_t _Nonnull self)
{
    List dead_vps;
    struct timespec now, timeout, deadline;

    timespec_from_sec(&timeout, 1);
    
    while (true) {
        dead_vps = LIST_INIT;
        const int sps = preempt_disable();

        // Continue to wait as long as there's nothing to finalize
        while (List_IsEmpty(&self->finalizer_queue)) {
            (void)wq_timedwait(&g_sched_wq,
                                &SIGSET_IGNORE_ALL,
                                0,
                                &timeout,
                                NULL);
        }
        
        // Got some work to do. Save off the needed data in local vars and then
        // reenable preemption before we go and do the actual work.
        dead_vps = self->finalizer_queue;
        self->finalizer_queue = LIST_INIT;
        
        preempt_restore(sps);
        
        
        // XXX
        // XXX Should boost the priority of VPs that have been sitting on the
        // XXX ready queue for some time. Goal is to prevent a VP from getting
        // XXX starved to death because it's a very low priority one and there's
        // XXX always some higher priority VP sneaking in. Now a low priority
        // XXX VP which blocks will receive a boost once it is unblocked but this
        // XXX doesn't help background VPs which block rarely or not at all
        // XXX because blocking isn't a natural part of the algorithm they are
        // XXX executing. Eg a VP that scales an image in the background.
        // XXX Boost could be +1 priority every 1/4 second
        // XXX
        
        // Finalize VPs which have exited
        List_ForEach(&dead_vps, ListNode,
            vcpu_t cp = vcpu_from_owner_qe(pCurNode);

            vcpu_destroy(cp);
        );
    }
}

#if 0
static void sched_dump_rdyq_locked(sched_t _Nonnull self)
{
    for (int i = 0; i < VP_PRIORITY_COUNT; i++) {
        vcpu_t pCurVP = (vcpu_t)self->ready_queue.priority[i].first;
        
        while (pCurVP) {
            printf("{pri: %d}, ", pCurVP->priority);
            pCurVP = (vcpu_t)pCurVP->rewa_qe.next;
        }
    }
    printf("\n");
    for (int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        printf("%hhx, ", self->ready_queue.populated[i]);
    }
    printf("\n");
}
#endif


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Virtual Processor
////////////////////////////////////////////////////////////////////////////////

vcpu_t vcpu_current(void)
{
    return (vcpu_t) g_sched->running;
}

int vcpu_currentid(void)
{
    return g_sched->running->id;
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
static vcpu_t _Nonnull boot_vcpu_create(BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx)
{
    vcpu_t self = BootAllocator_Allocate(bap, sizeof(struct vcpu));
    memset(self, 0, sizeof(struct vcpu));


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = SIZE_KB(2);
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    vcpu_sched_params_t sp;
    sp.qos = VCPU_QOS_REALTIME;
    sp.priority = 7;
    vcpu_cominit(self, &sp);

    vcpu_context_t cl;
    cl.func = (VoidFunc_1)fn;
    cl.context = ctx;
    cl.ret_func = NULL;
    cl.kernelStackBase = pKernelStackBase;
    cl.kernelStackSize = kernelStackSize;
    cl.userStackSize = 0;
    cl.isUser = false;

    try_bang(vcpu_setcontext(self, &cl, false));
    self->suspension_count = 0;
    
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
    vcpu_t self = BootAllocator_Allocate(bap, sizeof(struct vcpu));
    memset(self, 0, sizeof(struct vcpu));


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = SIZE_KB(1);
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    vcpu_sched_params_t sp;
    sp.qos = VCPU_QOS_IDLE;
    sp.priority = -8;
    vcpu_cominit(self, &sp);

    vcpu_context_t cl;
    cl.func = (VoidFunc_1)idle_vcpu_run;
    cl.context = NULL;
    cl.ret_func = NULL;
    cl.kernelStackBase = pKernelStackBase;
    cl.kernelStackSize = kernelStackSize;
    cl.userStackSize = 0;
    cl.isUser = false;

    try_bang(vcpu_setcontext(self, &cl, true));

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
