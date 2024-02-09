;
;  ULock_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 8/12/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include "lowmem.i"
    include <abi/errno.i>

    xref _ULock_OnWait
    xref _ULock_WakeUp
    xref _VirtualProcessor_GetCurrentVpid

    xdef _ULock_TryLock
    xdef _ULock_Lock
    xdef _ULock_Unlock
    xdef _ULock_GetOwnerVpid


    clrso
ulock_value             so.l    1    ; bit #7 == 1 -> lock is in acquired state; bset#7 == 0 -> lock is available for aquisition
ulock_wait_queue_first  so.l    1
ulock_wait_queue_last   so.l    1
ulock_owner_vpid        so.l    1
ulock_SIZEOF            so


;-------------------------------------------------------------------------------
; errno_t ULock_TryLock(ULock* _Nonnull pLock)
; Attempts to acquire the lock. Returns EOK if successful and EBUSY if the lock
; is being held by another virtual processor.
_ULock_TryLock:
    inline
    cargs lta_saved_d7.l, lta_lock_ptr.l

    move.l  d7, -(sp)
    ; try a to acquire the lock. This will give us the lock if it isn't currently
    ; held by someone else
    move.l  lta_lock_ptr(sp), a0
    bset    #7, ulock_value(a0)
    bne.s   .lta_lock_is_busy

    ; acquired the lock
    jsr     _VirtualProcessor_GetCurrentVpid
    move.l  lta_lock_ptr(sp), a0
    move.l  d0, ulock_owner_vpid(a0)

    ; return EOK
    moveq.l #EOK, d0

.lta_done:
    move.l  (sp)+, d7
    rts

.lta_lock_is_busy:
    moveq.l #EBUSY, d0
    bra.s   .lta_done

    einline


;-------------------------------------------------------------------------------
; errno_t ULock_Lock(ULock* _Nonnull pLock, unsigned int options)
; Acquires the lock. Blocks the caller if the lock is not available.
_ULock_Lock:
    inline
    cargs la_saved_d7.l, la_lock_ptr.l, la_lock_options.l

    move.l  d7, -(sp)
    ; try a fast acquire. This means that we take advantage of the fact that
    ; the bset instruction is atomic with respect to IRQs. This will give us
    ; the lock if it isn't currently held by someone else
    move.l  la_lock_ptr(sp), a0
    bset    #7, ulock_value(a0)
    beq.s   .la_acquired_lock

    ; The lock is held by someone else - wait and then retry.
    ; First recheck the lock after we have disable preemption. we have to do this
    ; because the VP who held the lock may have dropped it by now but we are not
    ; yet on the wait queue and thus the other VP isn't able to wake us up and
    ; cause us to retry.
    DISABLE_PREEMPTION d7

.la_retry:
    move.l  la_lock_ptr(sp), a0
    bset    #7, ulock_value(a0)
    beq.s   .la_acquired_slow

    move.l  la_lock_options(sp), -(sp)
    move.l  a0, -(sp)
    jsr     _ULock_OnWait
    addq.l  #8, sp

    ; give up if the OnWait came back with an error
    tst.l   d0
    bne.s   .la_wait_failed

    bra.s   .la_retry

.la_acquired_slow:
    RESTORE_PREEMPTION d7

.la_acquired_lock:
    jsr     _VirtualProcessor_GetCurrentVpid
    move.l  la_lock_ptr(sp), a0
    move.l  d0, ulock_owner_vpid(a0)

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
; errno_t ULock_Unlock(ULock* _Nonnull pLock)
; Unlocks the lock.
_ULock_Unlock:
    inline
    cargs lr_saved_d7.l, lr_lock_ptr.l

    move.l  d7, -(sp)

    ; make sure that we actually own the lock before we attempt to unlock it
    jsr     _VirtualProcessor_GetCurrentVpid
    move.l  lr_lock_ptr(sp), a0
    move.l  ulock_owner_vpid(a0), d1
    cmp.l   d0, d1
    bne.s   .lr_does_not_own_error

    ; unlock the lock 
    clr.l   ulock_owner_vpid(a0)
    DISABLE_PREEMPTION d7

    ; release the lock
    bclr    #7, ulock_value(a0)

    ; move all the waiters back to the ready queue
    move.l  a0, -(sp)
    jsr     _ULock_WakeUp
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
; int Lock_GetOwnerVpid(Lock* _Nonnull pLock)
; Returns the ID of the virtual processor that is currently holding the lock.
; Zero is returned if none is holding the lock.
_ULock_GetOwnerVpid:
    inline
    cargs lgovpid_saved_d7.l, lgovpid_lock_ptr.l

    move.l  d7, -(sp)

    DISABLE_PREEMPTION d7
    move.l  lgovpid_lock_ptr(sp), a0

    ; check whether the lock is currently held by someone
    btst    #7, ulock_value(a0)
    bne.s   .lgovpid_is_locked
    moveq.l #0, d0
    bra.s   .lgovpid_done

.lgovpid_is_locked:
    move.l  ulock_owner_vpid(a0), d0

.lgovpid_done:
    RESTORE_PREEMPTION d7
    move.l  (sp)+, d7
    rts

    einline
