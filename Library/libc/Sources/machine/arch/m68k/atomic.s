;
;  machine/arch/m68k/atomic.s
;  libc
;
;  Created by Dietmar Planitzer on 6/25/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef _atomic_flag_test_and_set
    xdef _atomic_flag_clear


; bit #7 == 1 -> lock is in acquired state; bset#7 == 0 -> lock is available for acquisition


;-------------------------------------------------------------------------------
; bool atomic_flag_test_and_set(volatile atomic_flag* _Nonnull flag)
_atomic_flag_test_and_set:
    inline
    cargs aftas_flag_ptr.l

    move.l  aftas_flag_ptr(sp), a0
    bset    #7, (a0)
    bne.s   .L1
    moveq.l #0, d0
    rts

.L1:
    moveq.l #1, d0
    rts

    einline


;-------------------------------------------------------------------------------
; void atomic_flag_clear(volatile atomic_flag* _Nonnull flag)
_atomic_flag_clear:
    inline
    cargs afc_flag_ptr.l

    move.l  afc_flag_ptr(sp), a0
    bclr    #7, (a0)
    rts

    einline
