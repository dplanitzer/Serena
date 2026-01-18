;
;  atomic_kei.s
;  libc
;
;  Created by Dietmar Planitzer on 1/4/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    include <machine/amiga/kei.i>


    xdef _atomic_int_exchange
    xdef _atomic_int_compare_exchange_strong
    xdef _atomic_int_fetch_add
    xdef _atomic_int_fetch_sub
    xdef _atomic_int_fetch_or
    xdef _atomic_int_fetch_xor
    xdef _atomic_int_fetch_and


;-------------------------------------------------------------------------------
; int atomic_int_exchange(volatile atomic_int* _Nonnull p{a0}, int val{d1})
; IRQ safe
_atomic_int_exchange:
    inline
    cargs aiex_ptr.l, aiex_op.l
        move.l  aiex_ptr(sp), a0
        move.l  aiex_op(sp), d1
        dc.w    ATOMIC_INT_EXCHANGE
        rts
    einline


;-------------------------------------------------------------------------------
; bool atomic_int_compare_exchange_strong(volatile atomic_int* _Nonnull p{a0}, volatile atomic_int* _Nonnull expected{a1}, int desired{d1})
; IRQ safe
_atomic_int_compare_exchange_strong:
    inline
    cargs aices_ptr.l, aices_expected.l, aices_desired.l
        move.l  aices_ptr(sp), a0
        move.l  aices_expected(sp), a1
        move.l  aices_desired(sp), d1
        dc.w    ATOMIC_INT_COMPARE_EXCHANGE_STRONG
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_add(volatile atomic_int* _Nonnull p{a0}, int op{d1})
; IRQ safe
_atomic_int_fetch_add:
    inline
    cargs aifa_ptr.l, aifa_op.l
        move.l  aifa_ptr(sp), a0
        move.l  aifa_op(sp), d1
        dc.w    ATOMIC_INT_FETCH_ADD
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_sub(volatile atomic_int* _Nonnull p{a0}, int op{d1})
; IRQ safe
_atomic_int_fetch_sub:
    inline
    cargs aifs_ptr.l, aifs_op.l
        move.l  aifs_ptr(sp), a0
        move.l  aifs_op(sp), d1
        dc.w    ATOMIC_INT_FETCH_SUB
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_or(volatile atomic_int* _Nonnull p{a0}, int op{d1})
; IRQ safe
_atomic_int_fetch_or:
    inline
    cargs aifo_ptr.l, aifo_op.l
        move.l  aifo_ptr(sp), a0
        move.l  aifo_op(sp), d1
        dc.w    ATOMIC_INT_FETCH_OR
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_xor(volatile atomic_int* _Nonnull p{a0}, int op{d1})
; IRQ safe
_atomic_int_fetch_xor:
    inline
    cargs aifx_ptr.l, aifx_op.l
        move.l  aifx_ptr(sp), a0
        move.l  aifx_op(sp), d1
        dc.w    ATOMIC_INT_FETCH_XOR
        rts
    einline


;-------------------------------------------------------------------------------
; int atomic_int_fetch_and(volatile atomic_int* _Nonnull p{a0}, int op{d1})
; IRQ safe
_atomic_int_fetch_and:
    inline
    cargs aifan_ptr.l, aifan_op.l
        move.l  aifan_ptr(sp), a0
        move.l  aifan_op(sp), d0
        dc.w    ATOMIC_INT_FETCH_AND
        rts
    einline
