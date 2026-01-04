;
;  atomic.s
;  kernel
;
;  Created by Dietmar Planitzer on 1/3/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    include <hal/hw/m68k/lowmem.i>


    xdef _atomic_int_fetch_add
    xdef _atomic_int_fetch_sub


; Note that read-modify-write instructions are not supported by the Amiga hardware.
; See this discussion thread: https://eab.abime.net/showthread.php?t=58493


;-------------------------------------------------------------------------------
; int atomic_int_fetch_add(volatile atomic_int* _Nonnull p, int op)
; IRQ safe
_atomic_int_fetch_add:
    inline
    cargs aifa_ptr.l, aifa_op.l
        move.l  aifa_ptr(sp), a0
        DISABLE_INTERRUPTS d1
        move.l  (a0), d0
        move.l  d0, a1
        add.l   aifa_op(sp), d0
        move.l  d0, (a0)
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
        move.l  (a0), d0
        move.l  d0, a1
        sub.l   aifs_op(sp), d0
        move.l  d0, (a0)
        RESTORE_INTERRUPTS d1
        move.l  a1, d0
        rts
    einline
