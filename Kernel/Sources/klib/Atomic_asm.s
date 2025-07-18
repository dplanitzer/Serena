;
;  Atomic_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 3/15/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include <machine/lowmem.i>


    xdef _AtomicBool_Set
    xdef _AtomicInt_Add
    xdef _AtomicInt_Subtract


; Note that read-modify-write instructions are not supported by the Amiga hardware.
; See this discussion thread: https://eab.abime.net/showthread.php?t=58493


;-------------------------------------------------------------------------------
; AtomicBool AtomicBool_Set(volatile AtomicBool* _Nonnull pValue, bool newValue)
; Atomically assign 'newValue' to the atomic bool stored in the given memory
; location and returns the previous value.
; IRQ safe
_AtomicBool_Set:
    inline
    cargs bas_value_ptr.l, bas_new_value.l
        move.l  bas_value_ptr(sp), a0
        tst.l   bas_new_value(sp)
        bne.s   .L1
        bclr.b  #0, (a0)    ; atomically tests and clears
        bra.s   .L2
.L1:
        bset.b  #0, (a0)    ; atomically tests and sets
.L2:
        bne.s   .L3
        moveq.l #0, d0
        rts
.L3:
        moveq.l #1, d0
        rts
    einline


;-------------------------------------------------------------------------------
; AtomicInt AtomicInt_Add(volatile AtomicInt* _Nonnull pValue, int increment)
; Atomically adds the 'increment' value to the integer stored in the given
; memory location and returns the new value.
; IRQ safe
_AtomicInt_Add:
    inline
    cargs iaa_value_ptr.l, iaa_increment.l
        move.l  iaa_value_ptr(sp), a0
        DISABLE_PREEMPTION d1
        move.l  (a0), d0
        add.l   iaa_increment(sp), d0
        move.l  d0, (a0)
        RESTORE_PREEMPTION d1
        rts
    einline


;-------------------------------------------------------------------------------
; AtomicInt AtomicInt_Subtract(volatile AtomicInt* _Nonnull pValue, int decrement)
; Atomically subtracts the 'decrement' value from the integer stored in the given
; memory location and returns the new value.
; IRQ safe
_AtomicInt_Subtract:
    inline
    cargs ias_value_ptr.l, ias_decrement.l
        move.l  ias_value_ptr(sp), a0
        DISABLE_PREEMPTION d1
        move.l  (a0), d0
        sub.l   ias_decrement(sp), d0
        move.l  d0, (a0)
        RESTORE_PREEMPTION d1
        rts
    einline
