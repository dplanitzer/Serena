;
;  Atomic_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 3/15/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "lowmem.i"


    xdef _AtomicInt_Add
    xdef _AtomicInt_Increment
    xdef _AtomicInt_Decrement


;-------------------------------------------------------------------------------
; AtomicInt AtomicInt_Add(volatile AtomicInt* _Nonnull pValue, Int increment)
; Atomically adds the 'increment' value to the integer stored in the given
; memory location and returns the new value.
; IRQ routine safe
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
; AtomicInt AtomicInt_Increment(volatile AtomicInt* _Nonnull pValue)
; Atomically increments the integer in the given memory location and returns the
; new value.
; IRQ routine safe
_AtomicInt_Increment:
    inline
    cargs iai_value_ptr.l
        move.l  iai_value_ptr(sp), a0
        DISABLE_PREEMPTION d1
        move.l  (a0), d0
        addq.l  #1, d0
        move.l  d0, (a0)
        RESTORE_PREEMPTION d1
        rts
    einline


;-------------------------------------------------------------------------------
; Int AtomicInt_Decrement(volatile AtomicInt* _Nonnull pValue)
; Atomically decrements the integer in the given memory location and returns the
; new value.
; IRQ routine safe
_AtomicInt_Decrement:
    inline
    cargs iad_value_ptr.l
    move.l  iad_value_ptr(sp), a0
        DISABLE_PREEMPTION d1
        move.l  (a0), d0
        subq.l  #1, d0
        move.l  d0, (a0)
        RESTORE_PREEMPTION d1
        rts
    einline
