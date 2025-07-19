;
;  mtx_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 8/12/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include <machine/lowmem.i>
    include <machine/errno.i>

    xref _mtx_onwait
    xref _mtx_wake
    xref _vcpu_currentid

    xdef _mtx_trylock
    xdef _mtx_lock
    xdef __mtx_unlock
    xdef _mtx_ownerid


    clrso
lock_value              so.l    1    ; bit #7 == 1 -> lock is in acquired state; bset#7 == 0 -> lock is available for acquisition
lock_wait_queue_first   so.l    1
lock_wait_queue_last    so.l    1
lock_owner_vpid         so.l    1
lock_SIZEOF             so


;-------------------------------------------------------------------------------
; bool mtx_trylock(mtx_t* _Nonnull self)
; Attempts to acquire the lock. Returns true if successful and false if the lock
; is being held by another virtual processor.
_mtx_trylock:
    inline
    cargs lta_saved_d7.l, lta_lock_ptr.l

    move.l  d7, -(sp)
    ; try a to acquire the lock. This will give us the lock if it isn't currently
    ; held by someone else
    move.l  lta_lock_ptr(sp), a0
    bset    #7, lock_value(a0)
    bne.s   .lta_lock_is_busy

    ; acquired the lock
    jsr     _vcpu_currentid
    move.l  lta_lock_ptr(sp), a0
    move.l  d0, lock_owner_vpid(a0)

    ; return EOK
    moveq.l #1, d0

.lta_done:
    move.l  (sp)+, d7
    rts

.lta_lock_is_busy:
    moveq.l #0, d0
    bra.s   .lta_done

    einline


;-------------------------------------------------------------------------------
; errno_t mtx_lock(mtx_t* _Nonnull self)
; Acquires the lock. Blocks the caller if the lock is not available.
_mtx_lock:
    inline
    cargs la_saved_d7.l, la_lock_ptr.l

    move.l  d7, -(sp)
    ; try a fast acquire. This means that we take advantage of the fact that
    ; the bset instruction is atomic with respect to IRQs. This will give us
    ; the lock if it isn't currently held by someone else
    move.l  la_lock_ptr(sp), a0
    bset    #7, lock_value(a0)
    beq.s   .la_acquired_lock

    ; The lock is held by someone else - wait and then retry.
    ; First recheck the lock after we have disable preemption. we have to do this
    ; because the VP who held the lock may have dropped it by now but we are not
    ; yet on the wait queue and thus the other VP isn't able to wake us up and
    ; cause us to retry.
    DISABLE_PREEMPTION d7

.la_retry:
    move.l  la_lock_ptr(sp), a0
    bset    #7, lock_value(a0)
    beq.s   .la_acquired_slow

    move.l  a0, -(sp)
    jsr     _mtx_onwait
    addq.l  #4, sp

    ; give up if the OnWait came back with an error
    tst.l   d0
    bne.s   .la_wait_failed

    bra.s   .la_retry

.la_acquired_slow:
    RESTORE_PREEMPTION d7

.la_acquired_lock:
    jsr     _vcpu_currentid
    move.l  la_lock_ptr(sp), a0
    move.l  d0, lock_owner_vpid(a0)

    ; return EOK
    moveq.l #EOK, d0

.la_done:
    move.l  (sp)+, d7
    rts

.la_wait_failed:
    ; d0 holds the error code at this point
    RESTORE_PREEMPTION d7
    bra.s   .la_done

    einline


;-------------------------------------------------------------------------------
; errno_t _mtx_unlock(mtx_t* _Nonnull self)
; Unlocks the lock.
__mtx_unlock:
    inline
    cargs lr_saved_d7.l, lr_lock_ptr.l

    move.l  d7, -(sp)

    ; make sure that we actually own the lock before we attempt to unlock it
    jsr     _vcpu_currentid
    move.l  lr_lock_ptr(sp), a0
    move.l  lock_owner_vpid(a0), d1
    cmp.l   d0, d1
    bne.s   .lr_does_not_own_error

    ; unlock the lock 
    clr.l   lock_owner_vpid(a0)
    DISABLE_PREEMPTION d7

    ; release the lock
    bclr    #7, lock_value(a0)

    ; move all the waiters back to the ready queue
    move.l  a0, -(sp)
    jsr     _mtx_wake
    addq.l  #4, sp

    RESTORE_PREEMPTION d7
    moveq.l #EOK, d0

.lr_done:
    move.l  (sp)+, d7
    rts

.lr_does_not_own_error
    moveq.l #EPERM, d0
    bra.s   .lr_done

    einline


;-------------------------------------------------------------------------------
; int mtx_ownerid(mtx_t* _Nonnull self)
; Returns the ID of the virtual processor that is currently holding the lock.
; Zero is returned if none is holding the lock.
_mtx_ownerid:
    inline
    cargs lgovpid_saved_d7.l, lgovpid_lock_ptr.l

    move.l  d7, -(sp)

    DISABLE_PREEMPTION d7
    move.l  lgovpid_lock_ptr(sp), a0

    ; check whether the lock is currently held by someone
    btst    #7, lock_value(a0)
    bne.s   .lgovpid_is_locked
    moveq.l #0, d0
    bra.s   .lgovpid_done

.lgovpid_is_locked:
    move.l  lock_owner_vpid(a0), d0

.lgovpid_done:
    RESTORE_PREEMPTION d7
    move.l  (sp)+, d7
    rts

    einline
