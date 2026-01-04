;
;  machine/hw/m68k-amiga/irq_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 8/12/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include <hal/hw/m68k/lowmem.i>

    xref _g_irq_clock_func
    xref _g_irq_clock_arg
    xref _g_irq_key_func
    xref _g_irq_key_arg
    xref _g_irq_disk_block_func
    xref _g_irq_disk_block_arg

    xref _g_vbl_handlers;
    xref _g_int2_handlers;
    xref _g_int6_handlers;
    xref __irq_run_handlers

    xref _g_irq_stat_uninit
    xref _g_irq_stat_spurious
    xref _g_irq_stat_nmi

    xref _g_sched
    xref __sched_switch_context


    xdef _irq_set_mask
    xdef _irq_restore_mask
    xdef _irq_enable_src
    xdef _irq_disable_src

    xdef __irq_level_1
    xdef __irq_level_2
    xdef __irq_level_3
    xdef __irq_level_4
    xdef __irq_level_5
    xdef __irq_level_6
    xdef __irq_uninitialized
    xdef __irq_spurious
    xdef __irq_level_7


; IRQ sources
IRQ_ID_CIA_B_FLAG                   equ 23
IRQ_ID_CIA_B_SP                     equ 22
IRQ_ID_CIA_B_ALARM                  equ 21
IRQ_ID_CIA_B_TIMER_B                equ 20
IRQ_ID_CIA_B_TIMER_A                equ 19

IRQ_ID_CIA_A_FLAG                   equ 18
IRQ_ID_CIA_A_SP                     equ 17
IRQ_ID_CIA_A_ALARM                  equ 16
IRQ_ID_CIA_A_TIMER_B                equ 15
IRQ_ID_CIA_A_TIMER_A                equ 14

IRQ_ID_EXTERN                       equ 13
IRQ_ID_DISK_SYNC                    equ 12
IRQ_ID_SERIAL_RBF   equ 11
IRQ_ID_AUDIO3                       equ 10
IRQ_ID_AUDIO2                       equ 9
IRQ_ID_AUDIO1                       equ 8
IRQ_ID_AUDIO0                       equ 7
IRQ_ID_BLITTER                      equ 6
IRQ_ID_VBLANK               equ 5
IRQ_ID_COPPER                       equ 4
IRQ_ID_PORTS                        equ 3
IRQ_ID_SOFT                         equ 2
IRQ_ID_DISK_BLOCK                   equ 1
IRQ_ID_SERIAL_TBE equ 0

IRQ_ID_COUNT                        equ 24



;-------------------------------------------------------------------------------
; unsigned irq_set_mask(unsigned mask)
; Sets the CPU's interrupt priority mask to 'mask' and returns the previous mask.
; Calls to irq_set_mask() may be nested by pairing them with irq_restore_mask():
; first call sets the new mask and returns the previous mask and the second call
; in a pair restores the previously saved mask. This function only establishes
; the new mask if it is more restrictive than the currently active mask. It
; returns the currently active mask if any case. 
_irq_set_mask:
    cargs ism_saved_d2.l, ism_mask.l

    move.l  d2, -(sp)
    move.l  ism_mask(sp), d2

    move.w  sr, d0
    move.w  d0, d1
    and.l   #$0700, d0
    cmp.w   d2, d0
    bge.s   .1          ; skip SR update if old mask [d0] >= new mask [d2]

    and.w   #$f8ff, d1
    or.w    d2, d1
    move.w  d1, sr

.1:
    move.l  (sp)+, d2
    rts


;-------------------------------------------------------------------------------
; void irq_restore_mask(unsigned mask)
; Restores the CPU IRQ mask to 'mask'. 
_irq_restore_mask:
    cargs irm_mask.l

    move.l  irm_mask(sp), d1
    move.w  sr, d0
    and.w   #$f8ff, d0
    or.w    d1, d0
    move.w  d0, sr
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
    cmp.b   #IRQ_ID_EXTERN, d0
    bgt.s   cei_ciaa_interrupts
    lsl.w   d0, d1
    or.w    #(INTF_SETCLR | INTF_INTEN | INTF_EXTER | INTF_PORTS), d1
    move.w  d1, CUSTOM_BASE + INTENA
    bra.s   cei_done

cei_ciaa_interrupts:
    ; handle CIA A interrupts
    cmp.b   #IRQ_ID_CIA_A_FLAG, d0
    bgt.s   cei_ciab_interrupts
    sub.b   #IRQ_ID_CIA_A_TIMER_A, d0
    lsl.b   d0, d1
    bset    #ICRB_IR, d1
    move.b  d1, CIAAICR
    bra.s   cei_ciax_inten

cei_ciab_interrupts:
    ; handle CIA B interrupts
    cmp.b   #IRQ_ID_CIA_B_FLAG, d0
    bgt.s   cei_done
    sub.b   #IRQ_ID_CIA_B_TIMER_A, d0
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
    cmp.b   #IRQ_ID_EXTERN, d0
    bgt.s   cdi_ciaa_interrupts
    lsl.w   d0, d1
    bclr    #INTB_SETCLR, d1
    move.w  d1, CUSTOM_BASE+ INTENA
    bra.s   cdi_done

cdi_ciaa_interrupts:
    ; handle CIA A interrupts
    cmp.b   #IRQ_ID_CIA_A_FLAG, d0
    bgt.s   cdi_ciab_interrupts
    sub.b   #IRQ_ID_CIA_A_TIMER_A, d0
    lsl.b   d0, d1
    bclr    #ICRB_IR, d1
    move.b  d1, CIAAICR
    bra.s   cdi_done

cdi_ciab_interrupts:
    ; handle CIA B interrupts
    cmp.b   #IRQ_ID_CIA_B_FLAG, d0
    bgt.s   cdi_done
    sub.b   #IRQ_ID_CIA_B_TIMER_A, d0
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
    or.w    #$0700, sr      ; equal to DISABLE_INTERRUPTS
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
    nop
;    CALL_IRQ_HANDLERS irc_handlers_SERIAL_TRANSMIT_BUFFER_EMPTY

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
;    CALL_IRQ_HANDLERS irc_handlers_SOFT
    nop
    bra     irq_handler_done

;-------------------------------------------------------------------------------
; Level 2 IRQ handler (CIA A)
    align 4
__irq_level_2:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    move.b  CIAAICR, d7     ; implicitly acknowledges CIA A IRQs

    btst    #ICRB_TA, d7
    beq.s   irq_handler_ciaa_tb
;    CALL_IRQ_HANDLERS irc_handlers_CIA_A_TIMER_A
    nop

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
;    CALL_IRQ_HANDLERS irc_handlers_CIA_A_ALARM
    nop

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
;    CALL_IRQ_HANDLERS irc_handlers_CIA_A_FLAG
    nop

irq_handler_ports:
    move.w  CUSTOM_BASE + INTREQR, d7
    btst    #INTB_PORTS, d7
    beq     irq_handler_done

    move.w  #INTF_PORTS, CUSTOM_BASE + INTREQ
    move.l  _g_int2_handlers, -(sp)
    jsr     __irq_run_handlers
    addq.w  #4, sp
    bra     irq_handler_done

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
    move.l  _g_vbl_handlers, -(sp)
    jsr     __irq_run_handlers
    addq.w  #4, sp

irq_handler_blitter:
    btst    #INTB_BLIT, d7
    beq.s   irq_handler_copper
;    CALL_IRQ_HANDLERS irc_handlers_BLITTER
    nop

irq_handler_copper:
    btst    #INTB_COPER, d7
    beq     irq_handler_done
;    CALL_IRQ_HANDLERS irc_handlers_COPPER
    nop
    bra     irq_handler_done

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
;    CALL_IRQ_HANDLERS irc_handlers_AUDIO0
    nop

irq_handler_audio0:
    btst    #INTB_AUD0, d7
    beq.s   irq_handler_audio3
;    CALL_IRQ_HANDLERS irc_handlers_AUDIO1
    nop

irq_handler_audio3:
    btst    #INTB_AUD3, d7
    beq.s   irq_handler_audio1
;    CALL_IRQ_HANDLERS irc_handlers_AUDIO2
    nop

irq_handler_audio1:
    btst    #INTB_AUD1, d7
    beq     irq_handler_done
;    CALL_IRQ_HANDLERS irc_handlers_AUDIO3
    nop
    bra     irq_handler_done

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
;    CALL_IRQ_HANDLERS irc_handlers_SERIAL_RECEIVE_BUFFER_FULL
    nop

irq_handler_dsksync:
    btst    #INTB_DSKSYN, d7
    beq     irq_handler_done
;    CALL_IRQ_HANDLERS irc_handlers_DISK_SYNC
    nop
    bra     irq_handler_done

;-------------------------------------------------------------------------------
; Level 6 IRQ handler (CIA B)
    align 4
__irq_level_6:
    DISABLE_ALL_IRQS
    movem.l d0 - d1 / d7 / a0 - a1, -(sp)

    move.b  CIABICR, d7     ; implicitly acknowledges CIA B IRQs

    btst    #ICRB_TA, d7
    beq.s   irq_handler_ciab_tb
;    CALL_IRQ_HANDLERS irc_handlers_CIA_B_TIMER_A
    nop

irq_handler_ciab_tb:
    btst    #ICRB_TB, d7
    beq.s   irq_handler_ciab_alarm
;    CALL_IRQ_HANDLERS irc_handlers_CIA_B_TIMER_B
    nop

irq_handler_ciab_alarm:
    btst    #ICRB_ALRM, d7
    beq.s   irq_handler_ciab_sp
;    CALL_IRQ_HANDLERS irc_handlers_CIA_B_ALARM
    nop

irq_handler_ciab_sp:
    btst    #ICRB_SP, d7
    beq.s   irq_handler_ciab_flag
;    CALL_IRQ_HANDLERS irc_handlers_CIA_B_SP
    nop

irq_handler_ciab_flag:
    btst    #ICRB_FLG, d7
    beq.s   irq_handler_exter
;    CALL_IRQ_HANDLERS irc_handlers_CIA_B_FLAG
    nop

irq_handler_exter:
    move.w  CUSTOM_BASE + INTREQR, d7
    btst    #INTB_EXTER, d7
    beq.s   irq_handler_done

    move.w  #INTF_EXTER, CUSTOM_BASE + INTREQ
    move.l  _g_int6_handlers, -(sp)
    jsr     __irq_run_handlers
    addq.w  #4, sp

    ; FALL THROUGH


;-----------------------------------------------------------------------
; IRQ done
; check whether we should do a context switch. If not then just do a rte.
; Otherwise do the context switch which will implicitly do the rte.
irq_handler_done:
    move.l  _g_sched, a0
    btst    #CSWB_SIGNAL_SWITCH, sched_csw_signals(a0)
    movem.l (sp)+, d0 - d1 / d7 / a0 - a1
    bne.l   __sched_switch_context
    rte


;-------------------------------------------------------------------------------
; Uninitialized IRQ handler
__irq_uninitialized:
    addq.l  #1, _g_irq_stat_uninit
    rte


;-------------------------------------------------------------------------------
; Spurious IRQ handler
__irq_spurious:
    addq.l  #1, _g_irq_stat_spurious
    rte


;-------------------------------------------------------------------------------
; NMI handler
__irq_level_7:
    addq.l  #1, _g_irq_stat_nmi
    rte
