;
;  driver/hw/m68k-amiga/graphics/copper_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 9/16/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    include <machine/hw/m68k-amiga/chipset.i>
    include <machine/hw/m68k/lowmem.i>


    xdef _copper_unschedule
    xdef _sprite_ctl_submit
    xdef _sprite_ctl_cancel

    xref _g_copper_ready_prog
    xref _g_sprite_change_table
    xref _g_sprite_change_pending

;-------------------------------------------------------------------------------
; copper_prog_t _Nullable copper_unschedule(void)
; We disable all interrupts altogether while doing the unschedule to ensure that
; no context switch can happen while IRQs are off. Doing it this way is more
; efficient since we just want to execute a few instructions while locking out
; the VBL and CSW.
;
_copper_unschedule:
    inline
        move.w  sr, d1
        or.w    #$0700, sr
        move.l  _g_copper_ready_prog, d0
        clr.l   _g_copper_ready_prog
        move.w  d1, sr
        rts
    einline


;-------------------------------------------------------------------------------
; void sprite_ctl_submit(int spridx, void* _Nonnull sprptr, uint32_t ctl)
; IRQ masking: same as above.
;
_sprite_ctl_submit:
    inline
    cargs scs_spridx.l, scs_sprptr.l, scs_ctl.l
        move.l  scs_spridx(sp), d0              ; d0 := spridx
        lea     _g_sprite_change_table, a0
        lea     (a0, d0.l*8), a0                ; a0 := &g_sprite_change_table[spridx]

        swap    d0
        move.w  sr, d0
        swap    d0                              ; ; high 16 bit: saved SR; low 16 bit: spridx
        or.w    #$0700, sr
        
        move.l  scs_sprptr(sp), (a0)+           ; g_sprite_change_table[spridx].ptr = sprptr
        move.l  scs_ctl(sp), (a0)               ; g_sprite_change_table[spridx].ctl

        bset.b  d0, _g_sprite_change_pending    ; g_sprite_change_pending |= (1 << spridx);

        swap    d0
        move.w  d0, sr
        rts
    einline


;-------------------------------------------------------------------------------
; void sprite_ctl_cancel(int spridx)
; IRQ masking: same as above.
;
_sprite_ctl_cancel:
    inline
    cargs scc_spridx.l
        move.l  scc_spridx(sp), d0              ; d0 := spridx
        bclr.b  d0, _g_sprite_change_pending    ; no need to mask IRQs since only one instruction needed
        rts
    einline
