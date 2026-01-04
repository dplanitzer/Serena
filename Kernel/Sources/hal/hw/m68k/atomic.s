;
;  atomic.s
;  kernel
;
;  Created by Dietmar Planitzer on 1/3/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    include <hal/hw/m68k/lowmem.i>


    xdef _atomic_int_exchange
    xdef _atomic_int_fetch_add
    xdef _atomic_int_fetch_sub
    xdef _atomic_int_fetch_or
    xdef _atomic_int_fetch_xor
    xdef _atomic_int_fetch_and


; Note that read-modify-write instructions are not supported by the Amiga hardware.
; See this discussion thread: https://eab.abime.net/showthread.php?t=58493


;-------------------------------------------------------------------------------
; int atomic_int_exchange(volatile atomic_int* _Nonnull p, int val)
; IRQ safe
_atomic_int_exchange:
    inline
    cargs aiex_ptr.l, aiex_op.l
        move.l  aiex_ptr(sp), a0
        DISABLE_INTERRUPTS d1
        move.l  (a0), d0
        move.l  d0, a1
        move.l  aiex_op(sp), d0
        move.l  d0, (a0)
        RESTORE_INTERRUPTS d1
        move.l  a1, d0
        rts
    einline


;-------------------------------------------------------------------------------
; bool atomic_int_compare_exchange_strong(volatile atomic_int* _Nonnull p, volatile atomic_int* _Nonnull expected, int desired)
; IRQ safe
_atomic_int_compare_exchange_strong:
    inline
    cargs aices_ptr.l, aices_expected.l, aices_desired.l
        move.l  aices_ptr(sp), a0
        move.l  aices_expected(sp), a1

        DISABLE_INTERRUPTS d1
        move.l  (a0), d0
        cmp.l   (a1), d0
        bne.s   .not_same
        move.l  aices_desired(sp), d0
        move.l  d0, (a0)
        moveq.l #1, d0
        bra.s   .done
.not_same:
        move.l  d0, aices_expected(sp)
        moveq.l #0, d0
.done:
        RESTORE_INTERRUPTS d1
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_add(volatile atomic_int* _Nonnull p, int op)
; IRQ safe
_atomic_int_fetch_add:
    inline
    cargs aifa_ptr.l, aifa_op.l
        move.l  aifa_ptr(sp), a0
        DISABLE_INTERRUPTS d1
        move.l  (a0), a1
        move.l  aifa_op(sp), d0
        add.l   d0, (a0)
        RESTORE_INTERRUPTS d1
        move.l  a1, d0
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_sub(volatile atomic_int* _Nonnull p, int op)
; IRQ safe
_atomic_int_fetch_sub:
    inline
    cargs aifs_ptr.l, aifs_op.l
        move.l  aifs_ptr(sp), a0
        DISABLE_INTERRUPTS d1
        move.l  (a0), a1
        move.l  aifs_op(sp), d0
        sub.l   d0, (a0)
        RESTORE_INTERRUPTS d1
        move.l  a1, d0
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_or(volatile atomic_int* _Nonnull p, int op)
; IRQ safe
_atomic_int_fetch_or:
    inline
    cargs aifo_ptr.l, aifo_op.l
        move.l  aifo_ptr(sp), a0
        DISABLE_INTERRUPTS d1
        move.l  (a0), a1
        move.l  aifo_op(sp), d0
        or.l   d0, (a0)
        RESTORE_INTERRUPTS d1
        move.l  a1, d0
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_xor(volatile atomic_int* _Nonnull p, int op)
; IRQ safe
_atomic_int_fetch_xor:
    inline
    cargs aifx_ptr.l, aifx_op.l
        move.l  aifx_ptr(sp), a0
        DISABLE_INTERRUPTS d1
        move.l  (a0), a1
        move.l  aifx_op(sp), d0
        eor.l   d0, (a0)
        RESTORE_INTERRUPTS d1
        move.l  a1, d0
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_and(volatile atomic_int* _Nonnull p, int op)
; IRQ safe
_atomic_int_fetch_and:
    inline
    cargs aifan_ptr.l, aifan_op.l
        move.l  aifan_ptr(sp), a0
        DISABLE_INTERRUPTS d1
        move.l  (a0), a1
        move.l  aifan_op(sp), d0
        and.l   d0, (a0)
        RESTORE_INTERRUPTS d1
        move.l  a1, d0
        rts
    einline
