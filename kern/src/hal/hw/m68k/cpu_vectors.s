;
;  machine/hw/m68k/cpu_vectors.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


;
; Trap, exception and interrupt handlers.
;

    include "lowmem.i"
    include <hal/errno.i>
    include <machine/amiga/kei.i>

    xref _Reset
    xref _OnReset
    xref _OnBoot
    xref __sys_entry
    xref __sys_no_entry
    xref _cpu_exception
    xref _cpu_exception_return
    xref _cpu_halt

    xref __irq_level_1
    xref __irq_level_2
    xref __irq_level_3
    xref __irq_level_4
    xref __irq_level_5
    xref __irq_level_6
    xref __irq_uninitialized
    xref __irq_spurious
    xref __irq_level_7

    xref _g_sched
    xref __syscall_handler

    xref __atomic_int_exchange_reg
    xref __atomic_int_compare_exchange_strong_reg
    xref __atomic_int_fetch_add_reg
    xref __atomic_int_fetch_sub_reg
    xref __atomic_int_fetch_or_reg
    xref __atomic_int_fetch_xor_reg
    xref __atomic_int_fetch_and_reg

    xref __rc_release_reg


    xdef _cpu_vector_table
    xdef _excpt_return

    xdef _sigurgent
    xdef _sigurgent_end



; Kernel cpu vector table for 68k CPUs
_cpu_vector_table:
    dc.l RESET_STACK_BASE               ; 00, Reset SSP
    dc.l _Reset                         ; 01, Reset PC
    dc.l __cpu_exception                ; 02, Bus error / Access fault
    dc.l __cpu_exception                ; 03, Address error
    dc.l __cpu_exception                ; 04, Illegal Instruction
    dc.l __cpu_exception                ; 05, Zero Divide
    dc.l __cpu_exception                ; 06, CHK, CHK2 Instruction
    dc.l __cpu_exception                ; 07, cpTRAPcc, TRAPcc, TRAPV Instructions
    dc.l __cpu_exception                ; 08, Privilege Violation
    dc.l __cpu_exception                ; 09, Trace
    dc.l __line_a_exception             ; 10, Line 1010 Emulator (Unimplemented A-Line Opcode)
    dc.l __cpu_exception                ; 11, Line 1111 Emulator (Unimplemented F-Line Opcode) / Floating-Point Unimplemented Instruction / Floating-Point Disabled
    dc.l __cpu_exception                ; 12, Emulator Interrupt (68060)
    dc.l __cpu_exception                ; 13, Coprocessor Protocol Violation (68020 / 68030)
    dc.l __cpu_exception                ; 14, Format Error
    dc.l __irq_uninitialized            ; 15, Uninitialized Interrupt Vector
    dc.l __cpu_exception                ; 16, Reserved
    dc.l __cpu_exception                ; 17, Reserved
    dc.l __cpu_exception                ; 18, Reserved
    dc.l __cpu_exception                ; 19, Reserved
    dc.l __cpu_exception                ; 20, Reserved
    dc.l __cpu_exception                ; 21, Reserved
    dc.l __cpu_exception                ; 22, Reserved
    dc.l __cpu_exception                ; 23, Reserved
    dc.l __irq_spurious                 ; 24, Spurious Interrupt
    dc.l __irq_level_1                  ; 25, Level 1 (Soft-IRQ, Disk, Serial port)
    dc.l __irq_level_2                  ; 26, Level 2 (External INT2, CIAA)
    dc.l __irq_level_3                  ; 27, Level 3 (Blitter, VBL, Copper)
    dc.l __irq_level_4                  ; 28, Level 4 (Audio)
    dc.l __irq_level_5                  ; 29, Level 5 (Disk, Serial port)
    dc.l __irq_level_6                  ; 30, Level 6 (External INT6, CIAB)
    dc.l __irq_level_7                  ; 31, Level 7 (NMI - Unused)
    dc.l __sys_entry                    ; 32, Trap #0
    dc.l __cpu_exception_return         ; 33, Trap #1
    dc.l __sys_no_entry                 ; 34, Trap #2
    dc.l __sys_no_entry                 ; 35, Trap #3
    dc.l __cpu_exception                ; 36, Trap #4
    dc.l __cpu_exception                ; 37, Trap #5
    dc.l __cpu_exception                ; 38, Trap #6
    dc.l __cpu_exception                ; 39, Trap #7
    dc.l __cpu_exception                ; 40, Trap #8
    dc.l __cpu_exception                ; 41, Trap #9
    dc.l __cpu_exception                ; 42, Trap #10
    dc.l __cpu_exception                ; 43, Trap #11
    dc.l __cpu_exception                ; 44, Trap #12
    dc.l __cpu_exception                ; 45, Trap #13
    dc.l __cpu_exception                ; 46, Trap #14
    dc.l __cpu_exception                ; 47, Trap #15
    dc.l __cpu_exception                ; 48, FPCP Branch or Set on Unordered Condition
    dc.l __cpu_exception                ; 49, FPCP Inexact Result
    dc.l __cpu_exception                ; 50, FPCP Divide by Zero
    dc.l __cpu_exception                ; 51, FPCP Underflow
    dc.l __cpu_exception                ; 52, FPCP Operand Error
    dc.l __cpu_exception                ; 53, FPCP Overflow
    dc.l __cpu_exception                ; 54, FPCP Signaling NAN
    dc.l __cpu_exception                ; 55, FPCP Unimplemented Data Type (68040+)
    dc.l __cpu_exception                ; 56, PMMU Configuration Error (68851 / 68030)
    dc.l __cpu_exception                ; 57, PMMU Illegal Operation Error (68851)
    dc.l __cpu_exception                ; 58, PMMU Access Level Violation Error (68851)
    dc.l __cpu_exception                ; 59, Reserved
    dc.l __cpu_exception                ; 60, Unimplemented Effective Address (68060)
    dc.l __cpu_exception                ; 61, Unimplemented Integer Instruction (68060)
    dc.l __cpu_exception                ; 62, Reserved
    dc.l __cpu_exception                ; 63, Reserved
    dcb.l 192, __cpu_exception          ; 64, User-Defined Vectors (192)


;-------------------------------------------------------------------------------
; Line A exception handler
__line_a_exception:
    movem.l d2/a2, -(sp)
    move.w  ([10, sp]), d2          ; load PC pointing to the $Axxx instruction
    and.w   #$0fff, d2
    cmp.w   #ATOMIC_INSTR_LAST, d2
    bgt.s   .not_a_line_a_exception
    lea     __aline_table(pc, d2.w*4), a2
    jsr     ([a2])
    addq.l  #2, 10(sp)
    movem.l (sp)+, d2/a2
    rte

.not_a_line_a_exception:
    movem.l (sp)+, d2/a2
    bra.s   __cpu_exception

__aline_table:
    dc.l    __atomic_int_exchange_reg
    dc.l    __atomic_int_compare_exchange_strong_reg
    dc.l    __atomic_int_fetch_add_reg
    dc.l    __atomic_int_fetch_sub_reg
    dc.l    __atomic_int_fetch_or_reg
    dc.l    __atomic_int_fetch_xor_reg
    dc.l    __atomic_int_fetch_and_reg
    dc.l    __rc_release_reg


;-------------------------------------------------------------------------------
; Invokes the cpu_exception(vcpu_t _Nonnull vp, excpt_0_frame_t* _Nonnull utp) function.
; See 68020UM, p6-27
__cpu_exception:
    SAVE_CPU_STATE
    GET_CURRENT_VP a0
    SAVE_FPU_STATE a0
    move.l  sp, vp_excpt_sa(a0)

    ; Push a null RTE frame which will be used as a trampoline to invoke the user space exception handler
    move.l  #0, -(sp)
    move.l  #0, -(sp)

    move.l  sp, -(sp)
    move.l  a0, -(sp)
    jsr     _cpu_exception
    addq.l  #8, sp

    rte


;-------------------------------------------------------------------------------
; Invoked by excpt_return() from user space. Returns from a user space exception
; handler. It does this by popping its RTE frame to expose the RTE frame from
; the original __cpu_exception() call. It then RTEs through this RTE frame.
;
; Stack at this point:
; __cpu_exception_return RTE frame
; cpu_savearea_t
; original __cpu_exception RTE frame
;
__cpu_exception_return:
    GET_CURRENT_VP a0
    move.l  a0, -(sp)
    jsr     _cpu_exception_return
    move.l  (sp)+, a0
    move.l  #0, vp_excpt_sa(a0)

    ; Pop the __cpu_exception_return RTE frame
    addq.l  #8, sp

    RESTORE_FPU_STATE a0
    RESTORE_CPU_STATE

    ; Return through the __cpu_exception RTE frame
    rte


;-------------------------------------------------------------------------------
; void excpt_return(void)
_excpt_return:
    trap    #1
    ; NOT REACHED


;-------------------------------------------------------------------------------
; System call entry point.
;
; NOTE: you are expected to use the _syscall() function to invoke a system call.
; You should not use a trap instruction.
;
; Layout of the user stack when using _syscall():
;
; Arguments are pushed from right to left on the stack. The left-most argument
; is the system call number. The system call result is returned in d0.
;
;
; INTERNAL
;
; Layout of the user stack when using trap #0:
;
; Arguments are pushed from right to left on the stack. The left-most argument
; is a 32bit dummy word. The dummy word is the return address of the _syscall()
; function.
;
; argN
; ...
; arg0
; system call number
; dummy long word / _syscall() return address
; ##### <--- usp
;
__sys_entry:
    inline
        SAVE_CPU_STATE

        GET_CURRENT_VP a1
        move.l  sp, vp_syscall_sa(a1)

        ; Invoke the system call handler. This function writes its function
        ; result to the system call save area
        move.l  usp, a0
        move.l  a0, -(sp)
        move.l  a1, -(sp)
        jsr     __syscall_handler
        addq.l  #8, sp

        RESTORE_CPU_STATE

        rte
    einline


__sys_no_entry:
    inline
        move.l  a1, -(sp)
        GET_CURRENT_VP a1
        move.l  #ENOSYS, vp_uerrno(a1)
        moveq.l #-1, d0
        move.l  (sp)+, a1
        rte
    einline


;-------------------------------------------------------------------------------
; void sigurgent(void)
; void sigurgent_end(void)
_sigurgent:
    move.l  #63, -(sp)   ; SC_sigurgent
    subq.l  #4, sp
    trap    #0
    addq.l  #8, sp
    rts
_sigurgent_end:
    nop
