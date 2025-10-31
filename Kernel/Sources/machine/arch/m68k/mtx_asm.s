;
;  mtx_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 8/12/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include <machine/lowmem.i>
    include <machine/errno.i>

    xref _g_sched
    xref _mtx_onwait
    xref _mtx_wake

    xdef _mtx_trylock
    xdef _mtx_lock
    xdef __mtx_unlock
    xdef _mtx_owner


    clrso
mtx_value               so.l    1    ; bit #7 == 1 -> lock is in acquired state; bset#7 == 0 -> lock is available for acquisition
mtx_wait_queue_first    so.l    1
mtx_wait_queue_last     so.l    1
mtx_owner               so.l    1
mtx_SIZEOF              so


;-------------------------------------------------------------------------------
; bool mtx_trylock(mtx_t* _Nonnull self)
; Attempts to acquire the mutex. Returns true if successful and false if the
; mutex is being held by another virtual processor.
_mtx_trylock:
    inline
    cargs tl_mtx_ptr.l

    ; try to acquire the lock. This will give us the lock if it isn't currently
    ; held by someone else
    move.l  tl_mtx_ptr(sp), a0
    bset    #7, mtx_value(a0)
    bne.s   .tl_mtx_is_busy

    ; update mutex ownership info
    GET_CURRENT_VP a1
    move.l  a1, mtx_owner(a0)

    ; return EOK
    moveq.l #1, d0
    rts

.tl_mtx_is_busy:
    moveq.l #0, d0
    rts

    einline


;-------------------------------------------------------------------------------
; errno_t mtx_lock(mtx_t* _Nonnull self)
; Acquires the mutex. Blocks the caller while the mutex is not available.
_mtx_lock:
    inline
    cargs l_saved_d7.l, l_mtx_ptr.l

    move.l  d7, -(sp)

    ; try a fast acquire. This means that we take advantage of the fact that
    ; the bset instruction is atomic with respect to IRQs. This will give us
    ; the mutex if it isn't currently held by someone else
    move.l  l_mtx_ptr(sp), a0
    bset    #7, mtx_value(a0)
    beq.s   .l_acquired_mutex

    ; The mutex is held by someone else - wait and then retry.
    ; First recheck the mutex after we have disable preemption. we have to do this
    ; because the VP who held the mutex may have dropped it by now but we are not
    ; yet on the wait queue and thus the other VP isn't able to wake us up and
    ; cause us to retry.
    DISABLE_PREEMPTION d7

.l_retry:
    move.l  l_mtx_ptr(sp), a0
    bset    #7, mtx_value(a0)
    beq.s   .l_acquired_slow

    move.l  a0, -(sp)
    jsr     _mtx_onwait
    addq.l  #4, sp

    ; give up if the OnWait came back with an error
    tst.l   d0
    bne.s   .l_wait_failed

    bra.s   .l_retry

.l_acquired_slow:
    RESTORE_PREEMPTION d7

.l_acquired_mutex:
    GET_CURRENT_VP a1
    move.l  l_mtx_ptr(sp), a0
    move.l  a1, mtx_owner(a0)

    moveq.l #EOK, d0
    move.l  (sp)+, d7
    rts

.l_wait_failed:
    ; d0 holds the error code at this point
    RESTORE_PREEMPTION d7
    move.l  (sp)+, d7
    rts

    einline


;-------------------------------------------------------------------------------
; errno_t _mtx_unlock(mtx_t* _Nonnull self)
; Unlocks the mutex.
__mtx_unlock:
    inline
    cargs u_saved_d7.l, u_mtx_ptr.l

    move.l  d7, -(sp)

    ; make sure that we actually own the mutex before we attempt to unlock it
    GET_CURRENT_VP a1
    move.l  a1, d1
    move.l  u_mtx_ptr(sp), a0
    move.l  mtx_owner(a0), d0
    cmp.l   d0, d1
    bne.s   .u_does_not_own_error

    ; drop mutex ownership
    clr.l   mtx_owner(a0)
    DISABLE_PREEMPTION d7

    ; release the mutex
    bclr    #7, mtx_value(a0)

    ; move all the waiters back to the ready queue
    move.l  a0, -(sp)
    jsr     _mtx_wake
    addq.l  #4, sp

    RESTORE_PREEMPTION d7

    moveq.l #EOK, d0
    move.l  (sp)+, d7
    rts

.u_does_not_own_error
    moveq.l #EPERM, d0
    move.l  (sp)+, d7
    rts

    einline


;-------------------------------------------------------------------------------
; vcpu_t _Nullable mtx_owner(mtx_t* _Nonnull self)
; Returns the virtual processor that is currently holding the mutex.
; NULL is returned if none is holding the mutex.
_mtx_owner:
    inline
    cargs o_saved_d7.l, o_mtx_ptr.l

    move.l  d7, -(sp)

    DISABLE_PREEMPTION d7
    moveq.l #0, d0
    move.l  o_mtx_ptr(sp), a0

    ; check whether the mutex is currently held by someone
    btst    #7, mtx_value(a0)
    beq.s   .o_done
    move.l  mtx_owner(a0), d0

.o_done:
    RESTORE_PREEMPTION d7
    move.l  (sp)+, d7
    rts

    einline
