;
;  machine/amiga/irq_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 8/12/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    include <machine/amiga/chipset.i>
    include <machine/lowmem.i>

    xref _g_irq_clock_func
    xref _g_irq_clock_arg
    xref _g_irq_key_func
    xref _g_irq_key_arg
    xref _g_irq_disk_block_func
    xref _g_irq_disk_block_arg


    xref _InterruptController_OnInterrupt
    xref _gInterruptControllerStorage
    xref _g_sched_storage
    xref __csw_rte_switch


    xdef _irq_enable
    xdef _irq_disable
    xdef _irq_restore
    xdef _irq_enable_src
    xdef _irq_disable_src

    xdef __irq_level_1
    xdef __irq_level_2
    xdef __irq_level_3
    xdef __irq_level_4
    xdef __irq_level_5
    xdef __irq_level_6



;-------------------------------------------------------------------------------
; void irq_enable(void)
; Enables interrupt handling.
_irq_enable:
    and.w   #$f8ff, sr
    rts

;-------------------------------------------------------------------------------
; int irq_disable(void)
; Disables interrupt handling and returns the previous interrupt handling state.
; Returns the old IRQ state
_irq_disable:
    DISABLE_INTERRUPTS d0
    rts


;-------------------------------------------------------------------------------
; void irq_restore(int state)
; Restores the given interrupt handling state.
_irq_restore:
    cargs cri_state.l
    move.l  cri_state(sp), d0
    RESTORE_INTERRUPTS d0
    rts


;-------------------------------------------------------------------------------
; void irq_enable_src(int interruptId)
; Enables generation of the given interrupt type. This implicitly turns on the
; master IRQ switch.
; Note that we always turn on the master IRQ switch, external IRQs and the ports
; IRQs because it makes things simpler. External & ports IRQs are masked in the
; chips that are the source of an IRQ. CIA A/B chips.
_irq_enable_src:
    cargs cei_interrupt_id.l
    move.l  cei_interrupt_id(sp), d0

    moveq.l #0, d1
    bset    #0, d1

    ; handle chipset interrupts
    cmp.b   #IRQ_SOURCE_EXTERN, d0
    bgt.s   cei_ciaa_interrupts
    lsl.w   d0, d1
    or.w    #(INTF_SETCLR | INTF_INTEN | INTF_EXTER | INTF_PORTS), d1
    move.w  d1, CUSTOM_BASE + INTENA
    bra.s   cei_done

cei_ciaa_interrupts:
    ; handle CIA A interrupts
    cmp.b   #IRQ_SOURCE_CIA_A_FLAG, d0
    bgt.s   cei_ciab_interrupts
    sub.b   #IRQ_SOURCE_CIA_A_TIMER_A, d0
    lsl.b   d0, d1
    bset    #ICRB_IR, d1
    move.b  d1, CIAAICR
    bra.s   cei_ciax_inten

cei_ciab_interrupts:
    ; handle CIA B interrupts
    cmp.b   #IRQ_SOURCE_CIA_B_FLAG, d0
    bgt.s   cei_done
    sub.b   #IRQ_SOURCE_CIA_B_TIMER_A, d0
    lsl.b   d0, d1
    bset    #ICRB_IR, d1
    move.b  d1, CIABICR
    ; fall through

cei_ciax_inten:
    ; we enabled one of the CIA IRQs. We also need to turn on the master IRQ switch
    move.w #(INTF_SETCLR | INTF_INTEN | INTF_EXTER | INTF_PORTS), d1
    move.w  d1, CUSTOM_BASE + INTENA
    ; fall through

cei_done:
    rts


;-------------------------------------------------------------------------------
; void irq_disable_src(int interruptId)
; Disables the generation of the given interrupt type. This function leaves the
; master IRQ switch enabled. It doesn't matter 'cause we turn the IRQ source off
; anyway.
_irq_disable_src:
    cargs cdi_interrupt_id.l
    move.l  cdi_interrupt_id(sp), d0

    moveq.l #0, d1
    bset    #0, d1

    ; handle chipset interrupts
    cmp.b   #IRQ_SOURCE_EXTERN, d0
    bgt.s   cdi_ciaa_interrupts
    lsl.w   d0, d1
    bclr    #INTB_SETCLR, d1
    move.w  d1, CUSTOM_BASE+ INTENA
    bra.s   cdi_done

cdi_ciaa_interrupts:
    ; handle CIA A interrupts
    cmp.b   #IRQ_SOURCE_CIA_A_FLAG, d0
    bgt.s   cdi_ciab_interrupts
    sub.b   #IRQ_SOURCE_CIA_A_TIMER_A, d0
    lsl.b   d0, d1
    bclr    #ICRB_IR, d1
    move.b  d1, CIAAICR
    bra.s   cdi_done

cdi_ciab_interrupts:
    ; handle CIA B interrupts
    cmp.b   #IRQ_SOURCE_CIA_B_FLAG, d0
    bgt.s   cdi_done
    sub.b   #IRQ_SOURCE_CIA_B_TIMER_A, d0
    lsl.b   d0, d1
    bclr    #ICRB_IR, d1
    move.b  d1, CIABICR
    ; fall through

cdi_done:
    rts


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
; Level 1 IRQ handler
    align 4
__irq_level_1:
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
    move.l  _g_irq_disk_block_arg, -(sp)
    move.l  _g_irq_disk_block_func, a0
    jsr     (a0)
    addq.w  #4, sp

irq_handler_soft:
    btst    #INTB_SOFT, d7
    beq     irq_handler_done
    CALL_IRQ_HANDLERS irc_handlers_SOFT
    beq     irq_handler_done

;-------------------------------------------------------------------------------
; Level 2 IRQ handler (CIA A)
    align 4
__irq_level_2:
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
    move.b  CIAASDR, d0                         ; key press handler
    move.b  #61, CIAATALO                       ; pulse KDAT low for 85us
    move.b  #0, CIAATAHI
    move.b  #%01011001, CIAACRA
    not.b   d0
    ror.b   #1,d0
    extb.l  d0
    move.l  d0, -(sp)
    move.l  _g_irq_key_arg, -(sp)
    move.l  _g_irq_key_func, a0
    jsr     (a0)
    addq.w  #8, sp
irq_wait_key_ack:
    btst.b  #0, CIAACRA
    bne.s   irq_wait_key_ack
    and.b   #%10111111, CIAACRA

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
__irq_level_3:
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
__irq_level_4:
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
__irq_level_5:
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
__irq_level_6:
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
