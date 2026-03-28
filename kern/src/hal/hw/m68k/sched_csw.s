;
;  sched_csw.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/23/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "cpu.i
    include "lowmem.i"

    xref _g_sched
    xref _sched_set_ready
    xref _sched_set_unready
    xref _cpu_non_recoverable_error

    xdef _sched_highest_priority_ready
    xdef _sched_switch_context
    xdef _sched_switch_to_boot_vcpu
    xdef __sched_switch_context


;-------------------------------------------------------------------------------
; vcpu_t _Nullable sched_highest_priority_ready(sched_t _Nonnull self)
; Returns the highest priority vcpu ready for running. NULL if no vcpu is ready
; to run. Note that this happens if the machine is idle, teh idle vcpu is already
; scheduled and all higher priority vcpus are waiting for something. 
; @Entry Condition: preemption disabled
_sched_highest_priority_ready:
    movem.l d2 / a2-a3, -(sp)

    moveq   #31, d2
    move.l  (12 + 4)(sp), a3                                 ; a3 := self
    lea.l   sched_ready_queue(a3), a2                   ; a2 := &self->ready_queue.priority[0]
    move.l  (sched_ready_queue_populated + 2*4)(a3), d0 ; d0 := self->ready_queue.populated[2]
    bfffo   d0{0:32}, d1
    beq.s   .1
    move.w  d2, d0
    sub.w   d1, d0
    add.w   #64, d0
    move.l  (a2, d0.w*8), d0
    bra.s   .done

.1:
    move.l  (sched_ready_queue_populated + 1*4)(a3), d0 ; d0 := self->ready_queue.populated[1]
    bfffo   d0{0:32}, d1
    beq.s   .2
    move.w  d2, d0
    sub.w   d1, d0
    add.w   #32, d0
    move.l  (a2, d0.w*8), d0
    bra.s   .done

.2:
    move.l  (sched_ready_queue_populated + 0*4)(a3), d0 ; d0 := self->ready_queue.populated[0]
    bfffo   d0{0:32}, d1
    beq.s   .3
    sub.w   d1, d2
    move.l  (a2, d2.w*8), d0
    bra.s   .done

.3:
    moveq   #0, d0

.done:
    movem.l (sp)+, d2 / a2-a3
    rts




;-------------------------------------------------------------------------------
; void sched_switch_context(void)
; Invokes the context switcher. Expects that preemption is disabled and that the
; scheduler set up a CSW request. Enables preemption as it switches to another
; VP. Once this function returns to the caller preemption is disabled again.
_sched_switch_context:
    inline
        ; push a format $0 exception stack frame on the stack. The PC field in
        ; that frame points to our rts instruction.
        move.w  #0, -(sp)               ; format $0 exception stack frame
        lea     .csw_return(pc), a0
        move.l  a0, -(sp)               ; PC
        move.w  sr, -(sp)               ; SR
        jmp     __sched_switch_context

.csw_return:
        rts
    einline


;-------------------------------------------------------------------------------
; void sched_switch_to_boot_vcpu(void)
; Triggers the very first context switch to the boot virtual processor. This call
; transfers the CPU to the boot virtual processor execution context and does not
; return to the caller.
; Expects to be called with interrupts turned off.
_sched_switch_to_boot_vcpu:
    inline
        jmp     __csw_restore
        ; NOT REACHED
        move.l  #RGB_YELLOW, -(sp)
        jmp _cpu_non_recoverable_error
    einline


;-------------------------------------------------------------------------------
; void __sched_switch_context(void)
; Saves the CPU state of the currently running VP and restores the CPU state of
; the scheduled VP. Expects that it is called with a CPU exception stack frame
; on top of the kernel stack. Note that you really want to call this function
; with a jmp instruction.
;
; Exception stack frames & context switching:
;
; There are 2 ways to trigger a full context switch:
; 1) a timer interrupt
; 2) a call to _sched_switch_context()
; The CPU will push a format #0 exception stack frame on the kernel stack of the
; outgoing VP in the first case while the sched_switch_context() function pushes
; a handcrafted format #0 exception stack frame on the kernel stack before
; calling this function.
;
; The exception stack frame with the PC and SR values is preserved on the kernel
; stack of the outgoing VP and then the CPU integer and floating point states
; are saved off. The restore function then does the opposite: it restores the
; floating point and the integer state and it then returns with an RTE. This RTE
; then consumes the exception stack frame and reestablishes the PC and SR
; registers.
;
; There is one way to do a half context switch:
; 1) a call to _sched_switch_to_boot_vcpu()
; Since we are only running the restore half of the context switch in this case,
; the expectation of this function here is that someone already pushed an
; exception stack frame on the kernel stack of the VP we want to switch to.
; The _vcpu_reset_machine_state() function takes care of this by pushing a format #0
; CPU exception stack frame on the kernel stack of teh VP we want to switch to.
; 
; Expected base stack frame layout at entry:
; SP + 6: 2 bytes exception stack frame format indicator (usually $0)
; SP + 2: PC
; SP + 0: SR
__sched_switch_context:
    SAVE_CPU_STATE

    GET_CURRENT_VP a0

    SAVE_FPU_STATE a0

    ; save the cpu_saved_state pointer
    move.l  sp, vp_csw_sa(a0)

    ; move the current vp to the ready state and queue if its state hasn't already
    ; been changed to some other non-running state like waiting or suspended
    cmp.b   #VCPU_RUST_RUNNING, vp_run_state(a0)
    bne.s   __csw_restore

    move.l  _g_sched, a1

    ; Pass -1 (true) to sched_set_ready() if the preempt signal is cleared; 0
    ; (false) if the preempt signal is set.
    btst.b  #CSWB_SIGNAL_PREEMPTED, sched_csw_signals(a1)
    seq     d0
    extb.l  d0

    move.l  d0, -(sp)
    move.l  a0, -(sp)
    move.l  a1, -(sp)
    jsr     _sched_set_ready
    add.l   #12, sp


__csw_restore:
    move.l  _g_sched, a2

    ; restore the ksp from the cpu_saved_state pointer
    move.l  sched_scheduled(a2), a3
    move.l  vp_csw_sa(a3), sp

    ; clear the branch cache if necessary
    btst    #VP_FLAG_HAS_BC_BIT, vp_flags(a3)
    beq.s   .1
    ; XXX this code is written for a future where we'll got virtual memory fully implemented. Don't remove 
    movec   cacr, d0
    bset    #CACR_CUBC_BIT, d0
    movec   d0, cacr        ; clear all user cache entries (all user processes share teh same superuser space)


    ; it's safe to trash all registers here 'cause we'll override them anyway
    ; make the scheduled VP the running VP and clear out sched_scheduled
.1:
    moveq.l #-1, d0
    move.l  d0, -(sp)
    move.l  a3, -(sp)
    move.l  a2, -(sp)
    jsr     _sched_set_unready
    add.l   #12, sp

    move.l  a3, sched_running(a2)
    clr.l   sched_scheduled(a2)
    move.b  #0, sched_csw_signals(a2)

    RESTORE_FPU_STATE a3

    RESTORE_CPU_STATE

    rte
