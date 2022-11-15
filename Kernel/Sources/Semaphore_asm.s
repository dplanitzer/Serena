;
;  Semaphore_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/10/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "lowmem.i"

    xref _BinarySemaphore_OnWaitForPermit
    xref _BinarySemaphore_WakeUp
    xref _Semaphore_OnWaitForPermits
    xref _Semaphore_WakeUp

    xdef _BinarySemaphore_InitValue
    xdef _BinarySemaphore_Acquire
    xdef _BinarySemaphore_Release
    xdef _BinarySemaphore_TryAcquire

    xdef _Semaphore_AcquireMultiple
    xdef _Semaphore_AcquireAll
    xdef _Semaphore_ReleaseMultiple
    xdef _Semaphore_TryAcquireMultiple
    xdef _Semaphore_TryAcquireAll


    clrso
binsema_value              so.l    1    ; bit #7 == 1 -> semaphore is in acquired state; bset#7 == 0 -> semaphore is available for aquisition
binsema_wait_queue_first   so.l    1
binsema_wait_queue_last    so.l    1
binsema_SIZEOF             so


;-------------------------------------------------------------------------------
; void BinarySemaphore_InitValue(BinarySemaphore* _Nonnull pSemaphore, UInt val)
; Initializes the semaphore as acquired or available.
_BinarySemaphore_InitValue:
    inline
    cargs bsiv_sema_ptr.l, bsiv_val.l
    move.l  bsiv_sema_ptr(sp), a0
    clr.l   binsema_value(a0)

    tst.l   bsiv_val(sp)
    beq.s   .bsiv_done
    bset    #7, binsema_value(a0)

.bsiv_done:
    rts
    einline


;-------------------------------------------------------------------------------
; ErrorCode BinarySemaphore_Acquire(BinarySemaphore* _Nonnull pSemaphore, TimeInterval deadline)
; Acquires the semaphore. Blocks the caller if 'deadline' is in the future and
; the semaphore does not have a permit available.
; This function is guaranteed to not block if 'deadline' is in the past. Returns
; an error code indicating whether the function has timed out waiting for the
; semaphore to become available.
_BinarySemaphore_Acquire:
    inline
    cargs bsa_saved_d7.l, bsa_sema_ptr.l, bsa_deadline_secs.l, bsa_deadline_nanos.l
    move.l  d7, -(sp)
    ; try a fast acquire. This means that we take advantage of the fact that
    ; the bset instruction is atomic with respect to IRQs. This will give us
    ; the semaphore if it isn't currently held by someone else
    move.l  bsa_sema_ptr(sp), a0
    bset    #7, binsema_value(a0)
    beq.s   .bsa_acquired_fast

    ; semaphore is held by someone else - wait and then retry
    ; first recheck the semaphore after we have disable preemption. we have to
    ; do this because the VP who held the semaphore may have dropped it by now
    ; but we are not yet on the wait queue and thus the other VP isn't able to
    ; wake us up and cause us to retry.
    DISABLE_PREEMPTION d7

.bsa_retry:
    move.l  bsa_sema_ptr(sp), a0
    bset    #7, binsema_value(a0)
    beq.s   .bsa_acquired_slow

    move.l  bsa_deadline_secs(sp), d0
    move.l  bsa_deadline_nanos(sp), d1
    move.l  d1, -(sp)
    move.l  d0, -(sp)
    move.l  #SCHEDULER_BASE, -(sp)
    move.l  a0, -(sp)
    jsr     _BinarySemaphore_OnWaitForPermit
    add.l   #16, sp

    ; give up if the OnWaitForPermit came back with an error
    tst.l   d0
    bne.s   .bsa_wait_failed

    bra.s   .bsa_retry

.bsa_wait_failed:
    ; d0 has the error code
    RESTORE_PREEMPTION d7
    bra.s   .bsa_done

.bsa_acquired_slow:
    RESTORE_PREEMPTION d7

.bsa_acquired_fast:
    moveq.l #0, d0

.bsa_done:
    move.l  (sp)+, d7
    rts
    einline


;-------------------------------------------------------------------------------
; void BinarySemaphore_Release(BinarySemaphore* _Nonnull pSema)
; Releases the semaphore.
_BinarySemaphore_Release:
    cargs bsr_saved_d7.l, bsr_sema_ptr.l
    move.l  d7, -(sp)
    move.l  bsr_sema_ptr(sp), a0

    DISABLE_PREEMPTION d7

    ; release the semaphore
    bclr    #7, binsema_value(a0)

    ; move all the waiters back to the ready queue
    move.l  #SCHEDULER_BASE, -(sp)
    move.l  a0, -(sp)
    jsr     _BinarySemaphore_WakeUp
    addq.l  #8, sp

    RESTORE_PREEMPTION d7

    move.l  (sp)+, d7
    rts


;-------------------------------------------------------------------------------
; Bool BinarySemaphore_TryAcquire(BinarySemaphore* _Nonnull sema)
; Tries to acquire the permit from the binary semaphore. Returns true if the
; acquisition was successful and false otherwise. This function does not block.
_BinarySemaphore_TryAcquire:
    inline
    cargs bta_sema.l

        ; check whether value > 0 and consume the permit and return; otherwise return 0
        move.l  bta_sema(sp), a0
        bset    #7, binsema_value(a0)
        beq.s   .bsta_acquired_permit

        moveq.l #0, d0
        rts

.bsta_acquired_permit:
        moveq.l #-1, d0
        rts
    einline




    clrso
sema_value              so.l    1
sema_wait_queue_first   so.l    1
sema_wait_queue_last    so.l    1
sema_SIZEOF             so


;-------------------------------------------------------------------------------
; void Semaphore_ReleaseMultiple(Semaphore* _Nonnull sema, Int npermits)
; Releases 'npermits' to the semaphore.
_Semaphore_ReleaseMultiple:
    inline
    cargs sr_saved_d7.l, sr_sema_ptr.l, sr_npermits.l
        move.l  d7, -(sp)
        move.l  sr_sema_ptr(sp), a0

        ; update the semaphore value. NO need to wake anyone up if the sema value
        ; is still <= 0
        move.l  sr_npermits(sp), d0
        add.l   d0, sema_value(a0)
        ble.s   .sr_done

        ; move all the waiters back to the ready queue
        DISABLE_PREEMPTION d7
        move.l  #SCHEDULER_BASE, -(sp)
        move.l  a0, -(sp)
        jsr     _Semaphore_WakeUp
        addq.l  #8, sp
        RESTORE_PREEMPTION d7

.sr_done:
        move.l  (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; ErrorCode Semaphore_AcquireMultiple(Semaphore* _Nonnull sema, Int npermits, TimeInterval deadline)
; Acquires 'npermits' from the semaphore before the deadline 'deadline' is reached.
; This function blocks the caller if 'deadline' is in the future and the semaphore
; does not have enough permits available.
; This function is guaranteed to not block if 'deadline' is in the past. Returns
; an error code indicating whether the function has timed out waiting for the
; semaphore to become available.
_Semaphore_AcquireMultiple:
    inline
    cargs sa_saved_d7.l, sa_sema.l, sa_npermits.l, sa_deadline_secs.l, sa_deadline_nanos.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value >= npermits and consume the permits and return if that's the case. Otherwise wait and retry
.csa_retry:
        move.l  sa_sema(sp), a0
        move.l  sema_value(a0), d0
        cmp.l   sa_npermits(sp), d0
        bge.s   .csa_claim_permits

        ; wait for permits to arrive and then retry
        move.l  sa_deadline_secs(sp), d0
        move.l  sa_deadline_nanos(sp), d1
        move.l  d1, -(sp)
        move.l  d0, -(sp)
        move.l  #SCHEDULER_BASE, -(sp)
        move.l  a0, -(sp)
        jsr     _Semaphore_OnWaitForPermits
        add.l   #16, sp

        ; give up if the OnWaitForPermits came back with an error
        tst.l   d0
        bne.s   .csa_wait_failed

        bra.s   .csa_retry

.csa_wait_failed:
        ; d0 has the error code
        RESTORE_PREEMPTION d7
        bra.s   .csa_done

.csa_claim_permits:
        move.l  sa_npermits(sp), d0
        sub.l   d0, sema_value(a0)
        RESTORE_PREEMPTION d7
        moveq.l #0, d0

.csa_done:
        move.l  (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; ErrorCode Semaphore_AcquireAll(Semaphore* _Nonnull sema, TimeInterval deadline, Int* _Nonnull pOutPermitCount)
; Acquires all permits from the semaphore before the deadline 'deadline' is reached.
; This function blocks the caller if 'deadline' is in the future and the semaphore
; does not have any permits available.
; This function is guaranteed to not block if 'deadline' is in the past. Returns
; an error code indicating whether the function has timed out waiting for the
; semaphore to become available.
; The number of acquired permits is returned in 'pOutPermitsCount'
_Semaphore_AcquireAll:
    inline
    cargs saa_saved_d7.l, saa_sema.l, saa_deadline_secs.l, saa_deadline_nanos.l, saa_out_permits_count_ptr.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value > 0 and consume all permits and return if that's the case. Otherwise wait and retry
.csaa_retry:
        move.l  saa_sema(sp), a0
        move.l  sema_value(a0), d0
        tst.l   d0
        bgt.s   .csaa_claim_permits

        ; wait for permits to arrive and then retry
        move.l  saa_deadline_secs(sp), d0
        move.l  saa_deadline_nanos(sp), d1
        move.l  d1, -(sp)
        move.l  d0, -(sp)
        move.l  #SCHEDULER_BASE, -(sp)
        move.l  a0, -(sp)
        jsr     _Semaphore_OnWaitForPermits
        add.l   #16, sp

        ; give up if the OnWaitForPermits came back with an error
        tst.l   d0
        bne.s   .csaa_wait_failed

        bra.s   .csaa_retry

.csaa_wait_failed:
        ; d0 has the error code
        RESTORE_PREEMPTION d7
        bra.s   .csaa_done

.csaa_claim_permits:
        move.l  sema_value(a0), d0
        move.l  saa_out_permits_count_ptr(sp), a1
        move.l  d0, (a1)
        clr.l   sema_value(a0)
        RESTORE_PREEMPTION d7
        moveq.l #0, d0

.csaa_done:
        move.l  (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; Bool Semaphore_TryAcquireMultiple(Semaphore* _Nonnull sema, Int npermits)
; Tries to acquire 'npermits' from the semaphore. Returns true if the acquisition
; was successful and false otherwise. This function does not block.
_Semaphore_TryAcquireMultiple:
    inline
    cargs sta_saved_d7.l, sta_sema.l, sta_npermits.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value >= npermits and consume them all and return; otherwise return 0
        move.l  sta_sema(sp), a0
        move.l  sema_value(a0), d0
        cmp.l   sta_npermits(sp), d0
        bge.s   .csta_claim_permits

        moveq.l #0, d0
        bra.s   .csta_done

.csta_claim_permits:
        move.l  sa_npermits(sp), d0
        sub.l   d0, sema_value(a0)
        moveq.l #-1, d0

.csta_done:
        RESTORE_PREEMPTION d7
        move.l  (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; Int Semaphore_TryAcquireAll(Semaphore* _Nonnull sema)
; Drains all available permits from the semaphore. Returns how many permits the
; function was able to acquire. This function does not block. A value of 0 is
; returned if no permits were available; otherwise the number of acquired permits
; is returned.
_Semaphore_TryAcquireAll:
    inline
    cargs sd_saved_d7.l, sd_sema.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value > 0 and consume all permits and return; otherwise return 0
        move.l  sd_sema(sp), a0
        move.l  sema_value(a0), d0
        tst.l   d0
        bgt.s   .csd_claim_permits

        moveq.l #0, d0
        bra.s   .csd_done

.csd_claim_permits:
        clr.l   sema_value(a0)
        ; d0 holds the number of acquired permits

.csd_done:
        RESTORE_PREEMPTION d7
        move.l  (sp)+, d7
        rts
    einline
