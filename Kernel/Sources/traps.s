;
;  traps_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


;
; Trap, exception and interrupt handlers.
;

    include "chipset.i"
    include "lowmem.i"

    xref _fatalError
    xref _chipset_reset
    xref _OnReset
    xref _OnBoot
    xref __rtecall_VirtualProcessorScheduler_SwitchContext
    xref _SystemCallHandler
    xref _InterruptController_OnInterrupt
    xref _copper_run
    xref _cpu_return_from_call_as_user
    xref _cpu_get_model
    xref _fpu_get_model
    xref _SystemDescription_Init
    xref _gInterruptControllerStorage
    xref _gVirtualProcessorSchedulerStorage

    xdef _cpu_vector_table
    xdef _cpu_non_recoverable_error
    xdef _mem_non_recoverable_error


; ROM vector table for 68000 CPUs
_cpu_vector_table:
    dc.l RESET_STACK_BASE               ; 0,  Reset SSP
    dc.l _Reset                         ; 1,  Reset PC
    dc.l BusErrorHandler                ; 2,  Bus error
    dc.l AddressErrorHandler            ; 3,  Address error
    dc.l IllegalInstructionHandler      ; 4,  Illegal Instruction
    dc.l IntDivByZeroHandler            ; 5,  Divide by 0
    dc.l ChkHandler                     ; 6,  CHK Instruction
    dc.l TrapvHandler                   ; 7,  TRAPV Instruction
    dc.l PrivilegeHandler               ; 8,  Privilege Violation
    dc.l TraceHandler                   ; 9,  Trace
    dc.l Line1010Handler                ; 10, Line 1010 Emu
    dc.l Line1111Handler                ; 11, Line 1111 Emu
    dc.l IgnoreTrap                     ; 12, Reserved
    dc.l CoProcViolationHandler         ; 13, Coprocessor Protocol Violation
    dc.l FormatErrorHandler             ; 14, Format Error
    dc.l IgnoreTrap                     ; 15, Uninit. Int. Vector.
    dcb.l 8, IgnoreTrap                 ; 16, Reserved (8)
    dc.l IRQHandler_Spurious            ; 24, Spurious Interrupt
    dc.l IRQHandler_L1                  ; 25, Level 1 (Soft-IRQ, Disk, Serial port)
    dc.l IRQHandler_L2                  ; 26, Level 2 (External INT2, CIAA)
    dc.l IRQHandler_L3                  ; 27, Level 3 (Blitter, VBL, Copper)
    dc.l IRQHandler_L4                  ; 28, Level 4 (Audio)
    dc.l IRQHandler_L5                  ; 29, Level 5 (Disk, Serial port)
    dc.l IRQHandler_L6                  ; 30, Level 6 (External INT6, CIAB)
    dc.l IgnoreTrap                     ; 31, Level 7 (NMI - Unused)
    dc.l _SystemCallHandler             ; 32, Trap 0 System Call
    dc.l _cpu_return_from_call_as_user  ; 33, Trap 1 _cpu_return_from_call_as_user
    dc.l IgnoreTrap                     ; 34, Trap 2 (Unused)
    dc.l IgnoreTrap                     ; 35, Trap 3 (Unused)
    dc.l IgnoreTrap                     ; 36, Trap 4 (Unused)
    dc.l IgnoreTrap                     ; 37, Trap 5 (Unused)
    dc.l IgnoreTrap                     ; 38, Trap 6 (Unused)
    dc.l IgnoreTrap                     ; 39, Trap 7 (Unused)
    dc.l IgnoreTrap                     ; 40, Trap 8 (Unused)
    dc.l IgnoreTrap                     ; 41, Trap 9 (Unused)
    dc.l IgnoreTrap                     ; 42, Trap 10 (Unused)
    dc.l IgnoreTrap                     ; 43, Trap 11 (Unused)
    dc.l IgnoreTrap                     ; 44, Trap 12 (Unused)
    dc.l IgnoreTrap                     ; 45, Trap 13 (Unused)
    dc.l IgnoreTrap                     ; 46, Trap 14 (Unused)
    dc.l IgnoreTrap                     ; 47, Trap 15 (Unused)
    dc.l IgnoreTrap                     ; 48, FP Branch or Set on Unordered Condition
    dc.l IgnoreTrap                     ; 49, FP Inexact Result
    dc.l IgnoreTrap                     ; 50, FP Divide by Zero
    dc.l IgnoreTrap                     ; 51, FP Underflow
    dc.l IgnoreTrap                     ; 52, FP Operand Error
    dc.l IgnoreTrap                     ; 53, FP Overflow
    dc.l IgnoreTrap                     ; 54, FP Signaling NAN
    dc.l FpUnimpDataTypeHandler         ; 55, FP Unimplemented Data Type (Defined for MC68040)
    dc.l MMUConfigErrorHandler          ; 56, MMU Configuration Error
    dc.l MMUIllegalOpErrorHandler       ; 57, MMU Illegal Operation Error
    dc.l MMUAccessLevelErrorHandler     ; 58, MMU Access Level Violation Error
    dcb.l 6, IgnoreTrap                 ; 59, Reserved
    dcb.l 192, IgnoreTrap               ; 64, User Defined Vector (192)


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
        lea     CUSTOM_BASE, a0
        move.w  #$ff00, POTGO(a0)
        move.w  #$0000, COLOR00(a0)

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
        ; CPU: MC68030
        ; FPU: none
        jsr     _cpu_get_model
        cmp.b   #CPU_MODEL_68030, d0
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
; Default trap handler. This just returns.
    align 2
IgnoreTrap:
    rte


;-------------------------------------------------------------------------------
; Bus error handler
    align 2
BusErrorHandler:
    bra _mem_non_recoverable_error
    ; NOT REACHED


;-------------------------------------------------------------------------------
; Address error handler
    align 2
AddressErrorHandler:
    pea 0
    pea .address_error_msg
    jsr _fatalError
    ; NOT REACHED

.address_error_msg:
    dc.b    "Address error", 0


;-------------------------------------------------------------------------------
; Illegal instruction handler
    align 2
IllegalInstructionHandler:
    pea 0
    pea .illegal_instruction_msg
    jsr _fatalError
    ; NOT REACHED

.illegal_instruction_msg:
    dc.b    "Illegal instruction", 0


;-------------------------------------------------------------------------------
; Int divide by zero handler
    align 2
IntDivByZeroHandler:
    pea 0
    pea .int_div_zero_msg
    jsr _fatalError
    ; NOT REACHED

.int_div_zero_msg:
    dc.b    "Divide by 0", 0


;-------------------------------------------------------------------------------
; CHK instruction handler
    align 2
ChkHandler:
    pea 0
    pea .chk_msg
    jsr _fatalError
    ; NOT REACHED

.chk_msg:
    dc.b    "CHK", 0

;-------------------------------------------------------------------------------
; TRAPV instruction handler
    align 2
TrapvHandler:
    pea 0
    pea .trapv_msg
    jsr _fatalError
    ; NOT REACHED

.trapv_msg:
    dc.b    "TRAPV", 0


;-------------------------------------------------------------------------------
; Privilege handler
    align 2
PrivilegeHandler:
    pea 0
    pea .privilege_msg
    jsr _fatalError
    ; NOT REACHED

.privilege_msg:
    dc.b    "Privilege", 0


;-------------------------------------------------------------------------------
; Trace handler
    align 2
TraceHandler:
    pea 0
    pea .trace_msg
    jsr _fatalError
    ; NOT REACHED

.trace_msg:
    dc.b    "Trace", 0


;-------------------------------------------------------------------------------
; Line 1010 handler
    align 2
Line1010Handler:
    pea 0
    pea .line1010_msg
    jsr _fatalError
    ; NOT REACHED

.line1010_msg:
    dc.b    "Line 1010", 0


;-------------------------------------------------------------------------------
; Line 1111 handler
    align 2
Line1111Handler:
    pea 0
    pea .line1111_msg
    jsr _fatalError
    ; NOT REACHED

.line1111_msg:
    dc.b    "Line 1111", 0


;-------------------------------------------------------------------------------
; Coprocessor protocol violation handler
    align 2
CoProcViolationHandler:
    pea 0
    pea .coproc_violation_msg
    jsr _fatalError
    ; NOT REACHED

.coproc_violation_msg:
    dc.b    "CoPro Protocol Violation", 0


;-------------------------------------------------------------------------------
; Format error handler
    align 2
FormatErrorHandler:
    pea 0
    pea .format_error_msg
    jsr _fatalError
    ; NOT REACHED

.format_error_msg:
    dc.b    "Format Error", 0


;-------------------------------------------------------------------------------
; FP unimplemented data type handler
    align 2
FpUnimpDataTypeHandler:
    pea 0
    pea .fp_unimp_datatype_msg
    jsr _fatalError
    ; NOT REACHED

.fp_unimp_datatype_msg:
    dc.b    "FP Unimplemented Data Type", 0


;-------------------------------------------------------------------------------
; MMU configuration error handler
    align 2
MMUConfigErrorHandler:
    pea 0
    pea .mmu_config_error_msg
    jsr _fatalError
    ; NOT REACHED

.mmu_config_error_msg:
    dc.b    "MMU Configuartion", 0


;-------------------------------------------------------------------------------
; MMU illegal operation error handler
    align 2
MMUIllegalOpErrorHandler:
    pea 0
    pea .mmu_illegal_op_error_msg
    jsr _fatalError
    ; NOT REACHED

.mmu_illegal_op_error_msg:
    dc.b    "MMU Illegal Op", 0


;-------------------------------------------------------------------------------
; MMU access level violation error handler
    align 2
MMUAccessLevelErrorHandler:
    pea 0
    pea .mmu_access_level_error_msg
    jsr _fatalError
    ; NOT REACHED

.mmu_access_level_error_msg:
    dc.b    "MMU Access Level Violation", 0


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
    addq.l  #4, sp
    endm


;-------------------------------------------------------------------------------
; Spurious IRQ handler
    align 2
IRQHandler_Spurious:
    DISABLE_ALL_IRQS
    addq.l  #1, _gInterruptControllerStorage + irc_spuriousInterruptCount
    rte


;-------------------------------------------------------------------------------
; Level 1 IRQ handler
    align 4
IRQHandler_L1:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    addq.b  #1, _gInterruptControllerStorage + irc_isServicingInterrupt
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
    beq.s   irq_handler_L1_done
    CALL_IRQ_HANDLERS irc_handlers_SOFT

irq_handler_L1_done:
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    bra.l   irq_handler_done


;-------------------------------------------------------------------------------
; Level 2 IRQ handler (CIA A)
    align 4
IRQHandler_L2:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    addq.b  #1, _gInterruptControllerStorage + irc_isServicingInterrupt
    move.b  CIAAICR, d7     ; implicitly acknowledges CIA A IRQs
    move.w  #INTF_PORTS, CUSTOM_BASE + INTREQ

    btst    #ICRB_TA, d7
    beq.s   irq_handler_ciaa_tb
    CALL_IRQ_HANDLERS irc_handlers_CIA_A_TIMER_A

irq_handler_ciaa_tb:
    btst    #ICRB_TB, d7
    beq.s   irq_handler_ciaa_alarm
    CALL_IRQ_HANDLERS irc_handlers_CIA_A_TIMER_B

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
    beq.s   irq_handler_L2_done
    CALL_IRQ_HANDLERS irc_handlers_CIA_A_FLAG

irq_handler_L2_done:
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    bra.l   irq_handler_done


;-------------------------------------------------------------------------------
; Level 3 IRQ handler
    align 4
IRQHandler_L3:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    addq.b  #1, _gInterruptControllerStorage + irc_isServicingInterrupt
    lea     CUSTOM_BASE, a0
    move.w  INTREQR(a0), d7
    move.w  #(INTF_COPER | INTF_VERTB | INTF_BLIT), INTREQ(a0)

    btst    #INTB_VERTB, d7
    beq.s   irq_handler_copper
    ; Run the Copper scheduler now because we want to minimize the delay between
    ; vblank IRQ and kicking off the right Copper program. Most importantly we
    ; want to make sure that no IRQ handler callback is able to delay the start
    ; of the Copper program
    jsr     _copper_run
    CALL_IRQ_HANDLERS irc_handlers_VERTICAL_BLANK

irq_handler_copper:
    btst    #INTB_COPER, d7
    beq.s   irq_handler_blitter
    CALL_IRQ_HANDLERS irc_handlers_COPPER

irq_handler_blitter:
    btst    #INTB_BLIT, d7
    beq.s   irq_handler_L3_done
    CALL_IRQ_HANDLERS irc_handlers_BLITTER

irq_handler_L3_done:
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    bra.l    irq_handler_done


;-------------------------------------------------------------------------------
; Level 4 IRQ handler
    align 4
IRQHandler_L4:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    addq.b  #1, _gInterruptControllerStorage + irc_isServicingInterrupt
    lea     CUSTOM_BASE, a0
    move.w  INTREQR(a0), d7
    move.w  #(INTF_AUD0 | INTF_AUD1 | INTF_AUD2 | INTF_AUD3), INTREQ(a0)

    btst    #INTB_AUD0, d7
    beq.s   irq_handler_audio1
    CALL_IRQ_HANDLERS irc_handlers_AUDIO0

irq_handler_audio1:
    btst    #INTB_AUD1, d7
    beq.s   irq_handler_audio2
    CALL_IRQ_HANDLERS irc_handlers_AUDIO1

irq_handler_audio2:
    btst    #INTB_AUD2, d7
    beq.s   irq_handler_audio3
    CALL_IRQ_HANDLERS irc_handlers_AUDIO2

irq_handler_audio3:
    btst    #INTB_AUD3, d7
    beq.s   irq_handler_L4_done
    CALL_IRQ_HANDLERS irc_handlers_AUDIO3

irq_handler_L4_done:
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    bra.l    irq_handler_done


;-------------------------------------------------------------------------------
; Level 5 IRQ handler
    align 4
IRQHandler_L5:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    addq.b  #1, _gInterruptControllerStorage + irc_isServicingInterrupt
    lea     CUSTOM_BASE, a0
    move.w  INTREQR(a0), d7
    move.w  #(INTF_RBF | INTF_DSKSYN), INTREQ(a0)

    btst    #INTB_RBF, d7
    beq.s   irq_handler_dsksync
    CALL_IRQ_HANDLERS irc_handlers_SERIAL_RECEIVE_BUFFER_FULL

irq_handler_dsksync:
    btst    #INTB_DSKSYN, d7
    beq.s   irq_handler_L5_done
    CALL_IRQ_HANDLERS irc_handlers_DISK_SYNC

irq_handler_L5_done:
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    bra.l   irq_handler_done


;-------------------------------------------------------------------------------
; Level 6 IRQ handler (CIA B)
    align 4
IRQHandler_L6:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    addq.b  #1, _gInterruptControllerStorage + irc_isServicingInterrupt
    move.b  CIABICR, d7     ; implicitly acknowledges CIA B IRQs
    move.w  #INTF_EXTER, CUSTOM_BASE + INTREQ

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
    beq.s   irq_handler_L6_done
    CALL_IRQ_HANDLERS irc_handlers_CIA_B_FLAG

irq_handler_L6_done:
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    ; FALL THROUGH


;-----------------------------------------------------------------------
; IRQ done
; check whether we should do a context switch. If not then just do a rte.
; Otherwise do the context switch which will implicitly do the rte.
irq_handler_done:
    subq.b  #1, _gInterruptControllerStorage + irc_isServicingInterrupt
    btst    #0, (_gVirtualProcessorSchedulerStorage + vps_csw_signals)
    bne.s   irq_handler_do_csw
    rte

irq_handler_do_csw:
    jmp __rtecall_VirtualProcessorScheduler_SwitchContext
    ; NOT REACHED
