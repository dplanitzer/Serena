;
;  rc.s
;  kernel
;
;  Created by Dietmar Planitzer on 7/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    include "cpu.i"


    xdef _rc_release
    xdef __rc_release_reg


;-------------------------------------------------------------------------------
; int rc_release(volatile ref_count_t* _Nonnull rc)
; Atomically releases a single strong reference and adjusts the retain count
; accordingly. Returns true if the retain count has reached zero and the caller
; should destroy the associated resources.
; IRQ safe
_rc_release:
    inline
    cargs release_rc_ptr.l
        move.l  release_rc_ptr(sp), a0

__rc_release_reg:
        moveq.l #0, d0
        DISABLE_INTERRUPTS_SP
        subq.l  #1, (a0)
        seq     d0
        RESTORE_INTERRUPTS_SP
        and.b   #1, d0      ; make sure we return proper false and true values
        rts
    einline
