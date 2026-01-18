;
;  arch/m68k/atomic.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 6/25/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef _atomic_flag_test_and_set
    xdef _atomic_flag_clear

    xdef _atomic_int_store
    xdef _atomic_int_load


; bit #0 == 1 -> lock is in acquired state; bset#0 == 0 -> lock is available for acquisition


;-------------------------------------------------------------------------------
; bool atomic_flag_test_and_set(volatile atomic_flag* _Nonnull flag)
_atomic_flag_test_and_set:
    inline
    cargs aftas_flag_ptr.l

    moveq.l #0, d0
    move.l  aftas_flag_ptr(sp), a0
    bset    #0, (a0)
    sne     d0
    and.b   #1, d0      ; make sure we return 0 (false) or 1 (true)
    rts

    einline


;-------------------------------------------------------------------------------
; void atomic_flag_clear(volatile atomic_flag* _Nonnull flag)
_atomic_flag_clear:
    inline
    cargs afc_flag_ptr.l

    move.l  afc_flag_ptr(sp), a0
    bclr    #0, (a0)
    rts

    einline


;-------------------------------------------------------------------------------
; void atomic_int_set(volatile atomic_int* _Nonnull p, int val)
_atomic_int_store:
    inline
    cargs ais_ptr.l, ais_val.l

    move.l  ais_ptr(sp), a0
    move.l  ais_val(sp), d0
    move.l  d0, (a0)
    rts

    einline


;-------------------------------------------------------------------------------
; int atomic_int_load(volatile atomic_int* _Nonnull p)
_atomic_int_load:
    inline
    cargs ail_ptr.l

    move.l  ais_ptr(sp), a0
    move.l  (a0), d0
    rts

    einline
