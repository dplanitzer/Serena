;
;  csw.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/23/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include <machine/lowmem.i>

    xref _g_sched_storage
    xref _cpu_non_recoverable_error

    xdef _csw_switch
    xdef _csw_switch_to_boot_vcpu
    xdef __csw_rte_switch


;-------------------------------------------------------------------------------
; void csw_switch(void)
; Invokes the context switcher. Expects that preemption is disabled and that the
; scheduler set up a CSW request. Enables preemption as it switches to another
; VP. Once this function returns to the caller preemption is disabled again.
_csw_switch:
    inline
        ; push a format $0 exception stack frame on the stack. The PC field in
        ; that frame points to our rts instruction.
        move.w  #0, -(sp)               ; format $0 exception stack frame
        lea     .csw_return(pc), a0
        move.l  a0, -(sp)               ; PC
        move.w  sr, -(sp)               ; SR
        jmp     __csw_rte_switch

.csw_return:
        rts
    einline


;-------------------------------------------------------------------------------
; void csw_switch_to_boot_vcpu(void)
; Triggers the very first context switch to the boot virtual processor. This call
; transfers the CPU to the boot virtual processor execution context and does not
; return to the caller.
; Expects to be called with interrupts turned off.
_csw_switch_to_boot_vcpu:
    inline
        jmp     __csw_rte_restore
        ; NOT REACHED
        move.l  #RGB_YELLOW, -(sp)
        jmp _cpu_non_recoverable_error
    einline


;-------------------------------------------------------------------------------
; void __csw_rte_switch(void)
; Saves the CPU state of the currently running VP and restores the CPU state of
; the scheduled VP. Expects that it is called with an exception stack frame on
; top of the stack that is based on a format $0 frame. You want to call this
; function with a jmp instruction.
;
; Exception stack frames & context switching:
;
; There are 2 ways to trigger a full context switch:
; 1) a timer interrupt
; 2) a call to _csw_switch
; The CPU will push a format $0 exception stack frame on the kernel stack of the
; outgoing VP in the first case while the VPS_SwitchContext() function pushes a
; handcrafted format $0 exception stack frame on the kernel stack of the outgoing
; VP.
; This function here preserves the exception stack frame on the kernel stack of
; the outgoing VP. So it saves its SSP such that it still points to the bottom
; of the exception stack frame. The exception stack frame stays on the stack
; while the outgoing VP sits in ready or waiting state.
; The restore half of this function here then updates this exception stack frame
; with the PC and SR of the incoming VP. It then finally returns with a rte
; instruction which installs the PC and SR in the respective CPU registers and
; it removes the exception stack frame.
; So the overall principle is that a context switch trigger puts an exception
; stack frame on the stack and this frame remains on the kernel stack while the
; VP sits in ready or waiting state. The frame is updated and finally removed
; when we restore the VP to running state.
;
; There is one way to do a half context switch:
; 1) a call to _csw_switch_to_boot_vcpu
; Since we are only running the restore half of the context switch in this case,
; the expectation of this function here is that someone already pushed an
; exception stack frame on the kernel stack of the VP we want to switch to.
; That's why cpu_make_callout() pushes a dummy format $0 exception
; stack frame on the stack.
; 
; Expected base stack frame layout at entry:
; SP + 6: 2 bytes exception stack frame format indicator (usually $0)
; SP + 2: PC
; SP + 0: SR
__csw_rte_switch:
    ; save the integer state
    move.l  a0, (_g_sched_storage + vps_csw_scratch)
    move.l  (_g_sched_storage + vps_running), a0

    ; update the VP state to Ready if the state hasn't already been changed to
    ; some other non-Running state like Waiting by the higher-level code
    cmp.b   #SCHED_STATE_RUNNING, vp_sched_state(a0)
    bne.s   .1
    move.b  #SCHED_STATE_READY, vp_sched_state(a0)

.1:
    lea     vp_save_area(a0), a0

    ; save all registers including the ssp
    movem.l d0 - d7 / a0 - a7, (a0)
    move.l  (_g_sched_storage + vps_csw_scratch), cpu_a0(a0)

    ; save the usp
    move.l  usp, a1
    move.l  a1, cpu_usp(a0)

    ; save the pc and sr
    move.w  0(sp), cpu_sr(a0)
    move.l  2(sp), cpu_pc(a0)

    ; check whether we should save the FPU state
    btst    #CSWB_HW_HAS_FPU, _g_sched_storage + vps_csw_hw
    beq.s   __csw_rte_restore

    ; save the FPU state. Note that the 68060 fmovem.l instruction does not
    ; support moving > 1 register at a time
    fsave       cpu_fsave(a0)
    fmovem      fp0 - fp7, cpu_fp0(a0)
    fmovem.l    fpcr, cpu_fpcr(a0)
    fmovem.l    fpsr, cpu_fpsr(a0)
    fmovem.l    fpiar, cpu_fpiar(a0)

__csw_rte_restore:
    ; consume the CSW switch signal
    bclr    #CSWB_SIGNAL_SWITCH, _g_sched_storage + vps_csw_signals

    ; it's safe to trash all registers here 'cause we'll override them anyway
    ; make the scheduled VP the running VP and clear out vps_scheduled
    move.l  (_g_sched_storage + vps_scheduled), a0
    move.l  a0, (_g_sched_storage + vps_running)
    moveq.l #0, d0
    move.l  d0, (_g_sched_storage + vps_scheduled)

    ; update the state to Running
    move.b  #SCHED_STATE_RUNNING, vp_sched_state(a0)
    lea     vp_save_area(a0), a0

    ; check whether we should restore the FPU state
    btst    #CSWB_HW_HAS_FPU, _g_sched_storage + vps_csw_hw
    beq.s   .2

    ; restore the FPU state. Note that the 68060 fmovem.l instruction does not
    ; support moving > 1 register at a time
    fmovem      cpu_fp0(a0), fp0 - fp7
    fmovem.l    cpu_fpcr(a0), fpcr
    fmovem.l    cpu_fpsr(a0), fpsr
    fmovem.l    cpu_fpiar(a0), fpiar
    frestore    cpu_fsave(a0)

.2:
    ; restore the usp
    move.l  cpu_usp(a0), a1
    move.l  a1, usp

    ; restore the ssp
    move.l  cpu_a7(a0), sp
    
    ; update the exception stack frame stored on the stack of the incoming VP
    ; with the PC and SR of the incoming VP. The rte instruction below will then
    ; consume this stack frame and establish the new execution context.
    ; The lowest 8 bytes of the exception stack frame:
    ; SP + 6: 2 byte wide format indicator (usually $0)
    ; SP + 2: PC
    ; SP + 0: SR
    move.l  cpu_pc(a0), 2(sp)
    move.w  cpu_sr(a0), 0(sp)
    
    ; restore the integer state
    movem.l (a0), d0 - d7 / a0 - a6

    rte
