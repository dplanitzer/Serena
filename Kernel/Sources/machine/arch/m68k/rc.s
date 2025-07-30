;
;  rc.s
;  kernel
;
;  Created by Dietmar Planitzer on 7/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    include <machine/lowmem.i>


    xdef _rc_retain
    xdef _rc_release
    xdef _rc_getcount


; Note that read-modify-write instructions are not supported by the Amiga hardware.
; See this discussion thread: https://eab.abime.net/showthread.php?t=58493


;-------------------------------------------------------------------------------
; void rc_retain(volatile ref_count_t* _Nonnull rc)
; Atomically increments the retain count 'rc'.
; IRQ safe
_rc_retain:
    inline
    cargs retain_rc_ptr.l
        move.l  retain_rc_ptr(sp), a0
        addq.l  #1, (a0)
        rts
    einline


;-------------------------------------------------------------------------------
; int rc_release(volatile ref_count_t* _Nonnull rc)
; Atomically releases a single strong reference and adjusts the retain count
; accordingly. Returns true if the retain count has reached zero and the caller
; should destroy the associated resources.
; IRQ safe
_rc_release:
    inline
    cargs release_rc_ptr.l
        DISABLE_PREEMPTION d1
        move.l  release_rc_ptr(sp), a0
        move.l  (a0), d0
        subq.l  #1, d0
        bmi.s   .ret_false
        move.l  d0, (a0)
        beq.s   .ret_true

.ret_false:
        RESTORE_PREEMPTION d1
        moveq.l #0, d0
        rts

.ret_true:
        RESTORE_PREEMPTION d1
        moveq.l #1, d0
        rts
    einline

;-------------------------------------------------------------------------------
; int rc_getcount(volatile ref_count_t* _Nonnull rc)
; Returns a copy of the current retain count. This should be used for debugging
; purposes only.
; IRQ safe
_rc_getcount:
    inline
    cargs getcount_rc_ptr.l
        move.l  getcount_rc_ptr(sp), d0
        rts
    einline
