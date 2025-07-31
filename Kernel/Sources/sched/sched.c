//
//  sched.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/23/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "sched.h"
#include "vcpu.h"
#include <machine/clock.h>
#include <machine/csw.h>
#include <machine/InterruptController.h>
#include <machine/amiga/chipset.h>
#include <kern/timespec.h>
#include <log/Log.h>

// Hardware timer usage:
// Amiga: CIA_A_TIMER_B -> quantum ticks


static void sched_dump_rdyq_locked(sched_t _Nonnull self);

static vcpu_t _Nonnull boot_vcpu_create(BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx);
static vcpu_t _Nonnull idle_vcpu_create(BootAllocator* _Nonnull bap);


struct sched        g_sched_storage;
sched_t             g_sched = &g_sched_storage;
struct waitqueue    gSchedulerWaitQueue;           // The scheduler VP waits on this queue


// Initializes the virtual processor scheduler and sets up the boot virtual
// processor plus the idle virtual processor. The 'pFunc' function will be
// invoked in the context of the boot virtual processor and it will receive the
// 'pContext' argument. The first context switch from the machine reset context
// to the boot virtual processor context is triggered by calling the
// VirtualProcessorScheduler_IncipientContextSwitch() function. 
void sched_create(SystemDescription* _Nonnull sdp, BootAllocator* _Nonnull bap, VoidFunc_1 _Nonnull fn, void* _Nullable _Weak ctx)
{
    // Stored in the BSS. Thus starts out zeroed.
    sched_t self = &g_sched_storage;


    // Initialize the boot virtual processor
    self->boot_vp = boot_vcpu_create(bap, fn, ctx);


    // Initialize the idle virtual processor
    self->idle_vp = idle_vcpu_create(bap);


    // Initialize the scheduler
    if (sdp->fpu_model != FPU_MODEL_NONE) {
        self->csw_hw |= CSW_HW_HAS_FPU;
    }
    
    List_Init(&self->timeout_queue);
    wq_init(&gSchedulerWaitQueue);
    List_Init(&self->finalizer_queue);

    for (int i = 0; i < VP_PRIORITY_COUNT; i++) {
        List_Init(&self->ready_queue.priority[i]);
    }
    for (int i = 0; i < VP_PRIORITY_POP_BYTE_COUNT; i++) {
        self->ready_queue.populated[i] = 0;
    }
    sched_add_vcpu_locked(
        self,
        self->boot_vp,
        self->boot_vp->priority);
    
    self->running = NULL;
    self->scheduled = sched_highest_priority_ready(self);
    self->csw_signals |= CSW_SIGNAL_SWITCH;
    sched_remove_vcpu_locked(self, self->scheduled);
    
    assert(self->scheduled == self->boot_vp);
}

// Called from OnStartup() after the heap has been created. Finishes the scheduler
// initialization.
void sched_finish_boot(sched_t _Nonnull self)
{
    decl_try_err();
    struct timespec ts;
    
    timespec_from_ms(&ts, 250);
    self->quantums_per_quarter_second = clock_time2quantums(g_mono_clock, &ts, QUANTUM_ROUNDING_AWAY_FROM_ZERO);


    // Resume the idle virtual processor
    vcpu_resume(self->idle_vp, false);
}

// Adds the given virtual processor with the given effective priority to the
// ready queue and resets its time slice length to the length implied by its
// effective priority.
void sched_add_vcpu_locked(sched_t _Nonnull self, vcpu_t _Nonnull vp, int effectivePriority)
{
    assert(vp != NULL);
    assert(vp->rewa_qe.prev == NULL);
    assert(vp->rewa_qe.next == NULL);
    assert(vp->suspension_count == 0);
    
    vp->sched_state = SCHED_STATE_READY;
    vp->effectivePriority = effectivePriority;
    vp->quantum_allowance = QuantumAllowanceForPriority(vp->effectivePriority);
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
    
    sched_add_vcpu_locked(self, vp, vp->priority);
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

// Inserts the timeout entry of the given vp in the global timeout list at the
// appropriate place.
static void _arm_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    register sched_timeout_t* pt = NULL;
    register sched_timeout_t* ct = (sched_timeout_t*)self->timeout_queue.first;

    while (ct) {
        if (ct->deadline > vp->timeout.deadline) {
            break;
        }
        
        pt = ct;
        ct = (sched_timeout_t*)ct->queue_entry.next;
    }
    
    List_InsertAfter(&self->timeout_queue, &vp->timeout.queue_entry, &pt->queue_entry);
}

// Arms a timeout for the given virtual processor. This puts the VP on the timeout
// queue.
void sched_arm_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp, const struct timespec* _Nonnull deadline)
{
    vp->timeout.deadline = clock_time2quantums(g_mono_clock, deadline, QUANTUM_ROUNDING_AWAY_FROM_ZERO);
    vp->timeout.is_valid = true;
    
    _arm_timeout(self, vp);
}

// Cancels an armed timeout for the given virtual processor. Does nothing if
// no timeout is armed.
void sched_cancel_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    if (vp->timeout.is_valid) {
        List_Remove(&self->timeout_queue, &vp->timeout.queue_entry);
        vp->timeout.deadline = kQuantums_Infinity;
        vp->timeout.is_valid = false;
    }
}

// Suspends a scheduled timeout for the given virtual processor. Does nothing if
// no timeout is armed.
void sched_suspend_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp)
{
    if (vp->timeout.is_valid) {
        List_Remove(&self->timeout_queue, &vp->timeout.queue_entry);
    }
}

// Resumes a suspended timeout for the given virtual processor.
void sched_resume_timeout(sched_t _Nonnull self, vcpu_t _Nonnull vp, Quantums suspensionTime)
{
    if (vp->timeout.is_valid) {
        vp->timeout.deadline += __max(clock_getticks(g_mono_clock) - suspensionTime, 0);
        _arm_timeout(self, vp);
    }
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
            
            sched_add_vcpu_locked(self, pCurRunning, pCurRunning->priority);
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
    
    if (dead_vps_count >= FINALIZE_NOW_THRESHOLD && gSchedulerWaitQueue.q.first != NULL) {
        // The scheduler VP is currently waiting for work. Let's wake it up.
        wq_wakeone(&gSchedulerWaitQueue, self->boot_vp, WAKEUP_CSW, WRES_WAKEUP);
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
        List_Init(&dead_vps);
        const int sps = preempt_disable();

        // Continue to wait as long as there's nothing to finalize
        while (List_IsEmpty(&self->finalizer_queue)) {
            (void)wq_timedwait(&gSchedulerWaitQueue,
                                &SIGSET_IGNORE_ALL,
                                0,
                                &timeout,
                                NULL);
        }
        
        // Got some work to do. Save off the needed data in local vars and then
        // reenable preemption before we go and do the actual work.
        dead_vps = self->finalizer_queue;
        List_Deinit(&self->finalizer_queue);
        
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
    return (vcpu_t) g_sched_storage.running;
}

int vcpu_currentid(void)
{
    return g_sched_storage.running->id;
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
    // Stored in the BSS. Thus starts out zeroed.
    static struct vcpu gBootVirtualProcessorStorage;
    vcpu_t vp = &gBootVirtualProcessorStorage;


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = CPU_PAGE_SIZE;
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    vcpu_cominit(vp, VP_PRIORITY_HIGHEST);

    VirtualProcessorClosure cl;
    cl.func = (VoidFunc_1)fn;
    cl.context = ctx;
    cl.ret_func = NULL;
    cl.kernelStackBase = pKernelStackBase;
    cl.kernelStackSize = kernelStackSize;
    cl.userStackSize = 0;
    cl.isUser = false;

    try_bang(vcpu_setclosure(vp, &cl));
    vp->save_area.sr |= 0x0700;    // IRQs should be disabled by default
    vp->suspension_count = 0;
    
    return vp;
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
        // Stored in the BSS. Thus starts out zeroed.
    static struct vcpu gIdleVirtualProcessorStorage;
    vcpu_t vp = &gIdleVirtualProcessorStorage;


    // Allocate the boot virtual processor kernel stack
    const int kernelStackSize = CPU_PAGE_SIZE;
    char* pKernelStackBase = BootAllocator_Allocate(bap, kernelStackSize);


    // Create the VP
    vcpu_cominit(vp, VP_PRIORITY_LOWEST);

    VirtualProcessorClosure cl;
    cl.func = (VoidFunc_1)idle_vcpu_run;
    cl.context = NULL;
    cl.ret_func = NULL;
    cl.kernelStackBase = pKernelStackBase;
    cl.kernelStackSize = kernelStackSize;
    cl.userStackSize = 0;
    cl.isUser = false;

    try_bang(vcpu_setclosure(vp, &cl));

    return vp;
}

// Puts the CPU to sleep until an interrupt occurs. The interrupt will give the
// scheduler a chance to run some other virtual processor if one is ready.
static void idle_vcpu_run(void* _Nullable ctx)
{
    while (true) {
        cpu_sleep(gSystemDescription->cpu_model);
    }
}
