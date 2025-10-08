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
; the scheduled VP. Expects that it is called with a CPU exception stack frame
; on top of the kernel stack. Note that you really want to call this function
; with a jmp instruction.
;
; Exception stack frames & context switching:
;
; There are 2 ways to trigger a full context switch:
; 1) a timer interrupt
; 2) a call to _csw_switch
; The CPU will push a format #0 exception stack frame on the kernel stack of the
; outgoing VP in the first case while the csw_switch() function pushes a hand-
; crafted format #0 exception stack frame on the kernel stack before calling
; this function.
;
; The exception stack frame with the PC and SR values is preserved on the kernel
; stack of the outgoing VP and then the CPU integer and floating point states
; are saved off. The restore function then does the opposite: it restores the
; floating point and the integer state and it then returns with an RTE. This RTE
; then consumes the exception stack frame and reestablishes the PC and SR
; registers.
;
; There is one way to do a half context switch:
; 1) a call to _csw_switch_to_boot_vcpu
; Since we are only running the restore half of the context switch in this case,
; the expectation of this function here is that someone already pushed an
; exception stack frame on the kernel stack of the VP we want to switch to.
; The vcpu_setcontext() function takes care of this by pushing a format #0 CPU
; exception stack frame on the kernel stack of teh VP we want to switch to.
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

    ; check whether we should save the FPU state
    move.l  (_g_sched_storage + vps_running), a1
    btst    #VP_FLAG_FPU_BIT, vp_flags(a1)
    beq.s   __csw_rte_restore

    ; save the FPU state. Note that the 68060 fmovem.l instruction does not
    ; support moving > 1 register at a time
    fsave       cpu_fsave(a0)
    fmovem      fp0 - fp7, cpu_fp0(a0)
    fmovem.l    fpcr, cpu_fpcr(a0)
    fmovem.l    fpsr, cpu_fpsr(a0)
    fmovem.l    fpiar, cpu_fpiar(a0)

__csw_rte_restore:
    lea     _g_sched_storage, a2

    ; consume the CSW switch signal
    bclr    #CSWB_SIGNAL_SWITCH, vps_csw_signals(a2)

    ; it's safe to trash all registers here 'cause we'll override them anyway
    ; make the scheduled VP the running VP and clear out vps_scheduled
    move.l  vps_scheduled(a2), a1
    move.l  a1, vps_running(a2)
    clr.l   vps_scheduled(a2)

    ; update the state to Running
    move.b  #SCHED_STATE_RUNNING, vp_sched_state(a1)
    lea     vp_save_area(a1), a0

    ; check whether we should restore the FPU state
    btst    #VP_FLAG_FPU_BIT, vp_flags(a1)
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
    
    ; restore the integer state
    movem.l (a0), d0 - d7 / a0 - a6

    rte
