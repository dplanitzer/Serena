;
;  machine/arch/m68k/cpu_vectors.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


;
; Trap, exception and interrupt handlers.
;

    include <machine/amiga/chipset.i>
    include <machine/lowmem.i>

    xref _fatalError
    xref _chipset_reset
    xref _OnReset
    xref _OnBoot
    xref __csw_rte_switch
    xref _syscall_entry
    xref _nosyscall_entry
    xref _InterruptController_OnInterrupt
    xref _cpu_get_model
    xref _fpu_get_model
    xref _cpu_exception
    xref _cpu_exception_return
    xref _SystemDescription_Init
    xref _gInterruptControllerStorage
    xref _g_sched_storage
    xref __fatalException
    xref _g_irq_clock_func
    xref _g_irq_clock_arg

    xdef _cpu_vector_table
    xdef _cpu_non_recoverable_error
    xdef _mem_non_recoverable_error

    ; so that we can get the address information from the assembler
    xdef IRQHandler_L1
    xdef IRQHandler_L2
    xdef IRQHandler_L3
    xdef IRQHandler_L4
    xdef IRQHandler_L5
    xdef IRQHandler_L6


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
    dc.l __cpu_exception                ; 10, Line 1010 Emulator (Unimplemented A-Line Opcode)
    dc.l __cpu_exception                ; 11, Line 1111 Emulator (Unimplemented F-Line Opcode) / Floating-Point Unimplemented Instruction / Floating-Point Disabled
    dc.l __cpu_exception                ; 12, Emulator Interrupt (68060)
    dc.l __cpu_exception                ; 13, Coprocessor Protocol Violation (68020 / 68030)
    dc.l __cpu_exception                ; 14, Format Error
    dc.l IRQHandler_Unitialized         ; 15, Uninitialized Interrupt Vector
    dc.l __cpu_exception                ; 16, Reserved
    dc.l __cpu_exception                ; 17, Reserved
    dc.l __cpu_exception                ; 18, Reserved
    dc.l __cpu_exception                ; 19, Reserved
    dc.l __cpu_exception                ; 20, Reserved
    dc.l __cpu_exception                ; 21, Reserved
    dc.l __cpu_exception                ; 22, Reserved
    dc.l __cpu_exception                ; 23, Reserved
    dc.l IRQHandler_Spurious            ; 24, Spurious Interrupt
    dc.l IRQHandler_L1                  ; 25, Level 1 (Soft-IRQ, Disk, Serial port)
    dc.l IRQHandler_L2                  ; 26, Level 2 (External INT2, CIAA)
    dc.l IRQHandler_L3                  ; 27, Level 3 (Blitter, VBL, Copper)
    dc.l IRQHandler_L4                  ; 28, Level 4 (Audio)
    dc.l IRQHandler_L5                  ; 29, Level 5 (Disk, Serial port)
    dc.l IRQHandler_L6                  ; 30, Level 6 (External INT6, CIAB)
    dc.l IRQHandler_NMI                 ; 31, Level 7 (NMI - Unused)
    dc.l _syscall_entry                 ; 32, Trap #0
    dc.l __cpu_exception_return         ; 33, Trap #1
    dc.l _nosyscall_entry               ; 34, Trap #2
    dc.l _nosyscall_entry               ; 35, Trap #3
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
    dc.l __fpu_exception                ; 48, FPCP Branch or Set on Unordered Condition
    dc.l __fpu_exception                ; 49, FPCP Inexact Result
    dc.l __fpu_exception                ; 50, FPCP Divide by Zero
    dc.l __fpu_exception                ; 51, FPCP Underflow
    dc.l __fpu_exception                ; 52, FPCP Operand Error
    dc.l __fpu_exception                ; 53, FPCP Overflow
    dc.l __fpu_exception                ; 54, FPCP Signaling NAN
    dc.l __fpu_exception                ; 55, FPCP Unimplemented Data Type (68040+)
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
; Boot ROM Reset entry point
    align 2
_Reset:
    inline
        ; boot ROM off; power light dimmed
        move.b  #3, CIAADDRA
        move.b  #2, CIAAPRA

        ; make sure that all DMAs and IRQs are off
        jsr     _chipset_reset

        ; install the CPU vector table
        lea     _cpu_vector_table(pc), a0
        move.w  #(CPU_VECTORS_SIZEOF/4)-1, d0
        lea     CPU_VECTORS_BASE, a1
.L1:    move.l  (a0)+, (a1)+
        dbra    d0, .L1

        ; clear the system description
        lea     SYS_DESC_BASE, a0
        move.w  #(SYS_DESC_SIZEOF/4)-1, d0
        moveq.l #0, d1
.L2:    move.l  d1, (a0)+
        dbra    d0, .L2

        ; figure out which type of CPU this is. The required minimum is:
        ; CPU: MC68020
        ; FPU: none
        jsr     _cpu_get_model
        cmp.b   #CPU_MODEL_68020, d0
        bge     .L3
        jmp     _cpu_non_recoverable_error
.L3:

        ; Initialize the system description
        move.l  d0, -(sp)
        move.l  #BOOT_SERVICES_MEM_TOP, -(sp)
        pea     SYS_DESC_BASE
        jsr     _SystemDescription_Init
        add.l   #12, sp

        ; call the OnBoot(SystemDescription*) routine
        pea     SYS_DESC_BASE
        jsr     _OnBoot

        ; NOT REACHED
        jmp     _cpu_non_recoverable_error
    einline


;-------------------------------------------------------------------------------
; Invoked if we encountered a non-recoverable CPU error. Eg the CPU type isn't
; supported. Sets the screen color to yellow and halts the machine
    align 2
_cpu_non_recoverable_error:
    inline
        lea     CUSTOM_BASE, a0
        move.w  #$0ff0, COLOR00(a0)
.L1:    bra     .L1
    einline


;-------------------------------------------------------------------------------
; Invoked if we encountered a non-recoverable memory error. Eg a bus error. Sets
; the screen color to red and halts the machine
    align 2
_mem_non_recoverable_error:
    inline
        lea     CUSTOM_BASE, a0
        move.w  #$0f00, COLOR00(a0)
.L1:    bra     .L1
    einline


;-------------------------------------------------------------------------------
; Invokes the cpu_exception(excpt_frame_t* _Nonnull efp, void* _Nullable sfp) function.
; See 68020UM, p6-27
__cpu_exception:
    ; Push a word on the stack to indicate to __cpu_exception_return that it does
    ; not need to do a frestore
    move.w  #0, -(sp)

    ; Push a null RTE frame which will be used to invoke the user space exception handler
    move.l  #0, -(sp)
    move.l  #0, -(sp)

    movem.l d0 - d1 / a0 - a1, -(sp)
    
    move.l  #0, -(sp)
    pea     (4 + 16 + 8 + 2)(sp)
    jsr     _cpu_exception
    addq.w  #8, sp
    move.l  d0, (16 + 2)(sp)    ; update the pc in our null RTE that we pushed above

    movem.l (sp)+, d0 - d1 / a0 - a1
    rte


;-------------------------------------------------------------------------------
; Invokes the cpu_exception(excpt_frame_t* _Nonnull efp, void* _Nullable sfp) function.
; See 68881/68882UM, p5-11
__fpu_exception:
    inline
        fsave       -(sp)

        ; Push a word on the stack to indicate to __cpu_exception_return that it
        ; does have to do a frestore
        move.w      #$fbe, -(sp)

        ; Push a null RTE frame which will be used to invoke the user space exception handler
        move.l      #0, -(sp)
        move.l      #0, -(sp)

        movem.l     d0 - d1 / a0 - a1, -(sp)

        move.b      (16 + 8 + 2)(sp), d0
        beq.s       .L1
        clr.l       d0
        move.b      (16 + 8 + 2 + 1)(sp), d0    ; get exception frame size
        bset        #3, (16 + 8 + 2)(sp, d0)    ; set bit #27 of BIU

        pea         (16 + 8 + 2)(sp)
        pea         (4 + 16 + 8 + 2)(sp, d0)
        jsr         _cpu_exception
        addq.w      #8, sp
        move.l      d0, (16 + 2)(sp)    ; update the pc in our null RTE that we pushed above

.L1:    
        movem.l     (sp)+, d0 - d1 / a0 - a1
        rte
    einline


;-------------------------------------------------------------------------------
; Invoked by excpt_return() from user space. Returns from a user space exception
; handler. It does this by popping its RTE frame to expose the RTE frame from
; the original __cpu_exception() call. It then RTEs through this RTE frame.
;
; Stack at this point:
; __cpu_exception_return RTE frame
; frestore control word ($0 or $fbe)
; fpu state frame (if frestore control word == $fbe)
; __cpu_exception RTE frame
;
__cpu_exception_return:
    inline
        movem.l     d0 - d1 / a0 - a1, -(sp)
        jsr         _cpu_exception_return
        movem.l     (sp)+, d0 - d1 / a0 - a1

        addq.w      #8, sp      ; pop __cpu_exception_return RTE frame
        cmp.w       #0, (sp)+
        beq.s       .L1
        frestore    (sp)+
.L1:
        rte                     ; return through the __cpu_exception RTE frame
    einline


;-------------------------------------------------------------------------------
; We disable IRQs altogether inside of IRQ handlers because we do not supported
; nested IRQ handling. This is the same as disabling preemption. Preemption is
; re-enabled when we do the RTE. Note that the CPU has already saved the original
; status register contents on the stack
    macro DISABLE_ALL_IRQS
    or.w    #$0700, sr      ; equal to DISABLE_PREEMPTION
    endm

    macro CALL_IRQ_HANDLERS
    pea     _gInterruptControllerStorage + \1
    jsr     _InterruptController_OnInterrupt
    addq.w  #4, sp
    endm


;-------------------------------------------------------------------------------
; Uninitialized IRQ handler
    align 2
IRQHandler_Unitialized:
    addq.l  #1, _gInterruptControllerStorage + irc_uninitializedInterruptCount
    rte


;-------------------------------------------------------------------------------
; Spurious IRQ handler
    align 2
IRQHandler_Spurious:
    addq.l  #1, _gInterruptControllerStorage + irc_spuriousInterruptCount
    rte


;-------------------------------------------------------------------------------
; NMI handler
    align 2
IRQHandler_NMI:
    addq.l  #1, _gInterruptControllerStorage + irc_nonMaskableInterruptCount
    rte


;-------------------------------------------------------------------------------
; Level 1 IRQ handler
    align 4
IRQHandler_L1:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    lea     CUSTOM_BASE, a0
    move.w  INTREQR(a0), d7
    move.w  #(INTF_TBE | INTF_DSKBLK | INTF_SOFT), INTREQ(a0)

    btst    #INTB_TBE, d7
    beq.s   irq_handler_dskblk
    CALL_IRQ_HANDLERS irc_handlers_SERIAL_TRANSMIT_BUFFER_EMPTY

irq_handler_dskblk:
    btst    #INTB_DSKBLK, d7
    beq.s   irq_handler_soft
    CALL_IRQ_HANDLERS irc_handlers_DISK_BLOCK

irq_handler_soft:
    btst    #INTB_SOFT, d7
    beq     irq_handler_done
    CALL_IRQ_HANDLERS irc_handlers_SOFT
    beq     irq_handler_done

;-------------------------------------------------------------------------------
; Level 2 IRQ handler (CIA A)
    align 4
IRQHandler_L2:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    move.b  CIAAICR, d7     ; implicitly acknowledges CIA A IRQs

    btst    #ICRB_TA, d7
    beq.s   irq_handler_ciaa_tb
    CALL_IRQ_HANDLERS irc_handlers_CIA_A_TIMER_A

irq_handler_ciaa_tb:
    btst    #ICRB_TB, d7
    beq.s   irq_handler_ciaa_alarm
    pea     20(sp)              ; d0 - d1 / d7 / a0 - a1
    move.l  _g_irq_clock_arg, -(sp)
    move.l  _g_irq_clock_func, a0
    jsr     (a0)
    addq.w  #8, sp

irq_handler_ciaa_alarm:
    btst    #ICRB_ALRM, d7
    beq.s   irq_handler_ciaa_sp
    CALL_IRQ_HANDLERS irc_handlers_CIA_A_ALARM

irq_handler_ciaa_sp:
    btst    #ICRB_SP, d7
    beq.s   irq_handler_ciaa_flag
    CALL_IRQ_HANDLERS irc_handlers_CIA_A_SP

irq_handler_ciaa_flag:
    btst    #ICRB_FLG, d7
    beq.s   irq_handler_ports
    CALL_IRQ_HANDLERS irc_handlers_CIA_A_FLAG

irq_handler_ports:
    move.w  CUSTOM_BASE + INTREQR, d7
    btst    #INTB_PORTS, d7
    beq     irq_handler_done

    move.w  #INTF_PORTS, CUSTOM_BASE + INTREQ
    CALL_IRQ_HANDLERS irc_handlers_PORTS
    beq     irq_handler_done

;-------------------------------------------------------------------------------
; Level 3 IRQ handler
    align 4
IRQHandler_L3:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    lea     CUSTOM_BASE, a0
    move.w  INTREQR(a0), d7
    move.w  #(INTF_COPER | INTF_VERTB | INTF_BLIT), INTREQ(a0)

    btst    #INTB_VERTB, d7
    beq.s   irq_handler_blitter
    CALL_IRQ_HANDLERS irc_handlers_VERTICAL_BLANK

irq_handler_blitter:
    btst    #INTB_BLIT, d7
    beq.s   irq_handler_copper
    CALL_IRQ_HANDLERS irc_handlers_BLITTER

irq_handler_copper:
    btst    #INTB_COPER, d7
    beq     irq_handler_done
    CALL_IRQ_HANDLERS irc_handlers_COPPER
    beq     irq_handler_done

;-------------------------------------------------------------------------------
; Level 4 IRQ handler
    align 4
IRQHandler_L4:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    lea     CUSTOM_BASE, a0
    move.w  INTREQR(a0), d7
    move.w  #(INTF_AUD0 | INTF_AUD1 | INTF_AUD2 | INTF_AUD3), INTREQ(a0)

    btst    #INTB_AUD2, d7
    beq.s   irq_handler_audio0
    CALL_IRQ_HANDLERS irc_handlers_AUDIO0

irq_handler_audio0:
    btst    #INTB_AUD0, d7
    beq.s   irq_handler_audio3
    CALL_IRQ_HANDLERS irc_handlers_AUDIO1

irq_handler_audio3:
    btst    #INTB_AUD3, d7
    beq.s   irq_handler_audio1
    CALL_IRQ_HANDLERS irc_handlers_AUDIO2

irq_handler_audio1:
    btst    #INTB_AUD1, d7
    beq     irq_handler_done
    CALL_IRQ_HANDLERS irc_handlers_AUDIO3
    beq     irq_handler_done

;-------------------------------------------------------------------------------
; Level 5 IRQ handler
    align 4
IRQHandler_L5:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    lea     CUSTOM_BASE, a0
    move.w  INTREQR(a0), d7
    move.w  #(INTF_RBF | INTF_DSKSYN), INTREQ(a0)

    btst    #INTB_RBF, d7
    beq.s   irq_handler_dsksync
    CALL_IRQ_HANDLERS irc_handlers_SERIAL_RECEIVE_BUFFER_FULL

irq_handler_dsksync:
    btst    #INTB_DSKSYN, d7
    beq     irq_handler_done
    CALL_IRQ_HANDLERS irc_handlers_DISK_SYNC
    beq     irq_handler_done

;-------------------------------------------------------------------------------
; Level 6 IRQ handler (CIA B)
    align 4
IRQHandler_L6:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    move.b  CIABICR, d7     ; implicitly acknowledges CIA B IRQs

    btst    #ICRB_TA, d7
    beq.s   irq_handler_ciab_tb
    CALL_IRQ_HANDLERS irc_handlers_CIA_B_TIMER_A

irq_handler_ciab_tb:
    btst    #ICRB_TB, d7
    beq.s   irq_handler_ciab_alarm
    CALL_IRQ_HANDLERS irc_handlers_CIA_B_TIMER_B

irq_handler_ciab_alarm:
    btst    #ICRB_ALRM, d7
    beq.s   irq_handler_ciab_sp
    CALL_IRQ_HANDLERS irc_handlers_CIA_B_ALARM

irq_handler_ciab_sp:
    btst    #ICRB_SP, d7
    beq.s   irq_handler_ciab_flag
    CALL_IRQ_HANDLERS irc_handlers_CIA_B_SP

irq_handler_ciab_flag:
    btst    #ICRB_FLG, d7
    beq.s   irq_handler_exter
    CALL_IRQ_HANDLERS irc_handlers_CIA_B_FLAG

irq_handler_exter:
    move.w  CUSTOM_BASE + INTREQR, d7
    btst    #INTB_EXTER, d7
    beq.s   irq_handler_done

    move.w  #INTF_EXTER, CUSTOM_BASE + INTREQ
    CALL_IRQ_HANDLERS irc_handlers_EXTER

    ; FALL THROUGH


;-----------------------------------------------------------------------
; IRQ done
; check whether we should do a context switch. If not then just do a rte.
; Otherwise do the context switch which will implicitly do the rte.
irq_handler_done:
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    btst    #0, (_g_sched_storage + vps_csw_signals)
    bne.l   __csw_rte_switch
    rte
