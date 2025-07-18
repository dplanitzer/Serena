;
;  Semaphore_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/10/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include <machine/lowmem.i>

    xref _Semaphore_OnWaitForPermits
    xref _Semaphore_WakeUp
    xref _wq_wake_irq

    xdef _Semaphore_AcquireMultiple
    xdef _Semaphore_AcquireAll
    xdef _Semaphore_RelinquishMultiple
    xdef _Semaphore_RelinquishFromInterrupt
    xdef _Semaphore_TryAcquireMultiple
    xdef _Semaphore_TryAcquireAll


    clrso
sema_value              so.l    1
sema_wait_queue_first   so.l    1
sema_wait_queue_last    so.l    1
sema_SIZEOF             so


;-------------------------------------------------------------------------------
; void Semaphore_RelinquishMultiple(Semaphore* _Nonnull sema, int npermits)
; Releases 'npermits' permits to the semaphore.
_Semaphore_RelinquishMultiple:
    inline
    cargs sr_sema_ptr.l, sr_npermits.l
        move.l  sr_sema_ptr(sp), a0

        DISABLE_PREEMPTION d0

        ; update the semaphore value. NO need to wake anyone up if the sema value
        ; is still <= 0
        move.l  sr_npermits(sp), d1
        add.l   d1, sema_value(a0)
        ble.s   .sr_done

        ; move all the waiters back to the ready queue
        move.l  d0, -(sp)
        move.l  a0, -(sp)
        jsr     _Semaphore_WakeUp
        addq.l  #4, sp
        move.l  (sp)+, d0

.sr_done:
        RESTORE_PREEMPTION d0
        rts
    einline


;-------------------------------------------------------------------------------
; void Semaphore_RelinquishFromInterrupt(Semaphore* _Nonnull sema)
; Releases one permit to the semaphore. This function expects to be called from
; the interrupt context and thus it does not trigger an immediate context switch
; since context switches are deferred until we return from the interrupt.
_Semaphore_RelinquishFromInterrupt:
    inline
    cargs srfic_sema_ptr.l
        move.l  srfic_sema_ptr(sp), a0

        DISABLE_PREEMPTION d0

        ; update the semaphore value. NO need to wake anyone up if the sema value
        ; is still <= 0
        addq.l  #1, sema_value(a0)
        ble.s   .srfic_done

        ; move all the waiters back to the ready queue
        move.l  d0, -(sp)
        pea     sema_wait_queue_first(a0)
        jsr     _wq_wake_irq
        addq.l  #4, sp
        move.l  (sp)+, d0

.srfic_done:
        RESTORE_PREEMPTION d0
        rts
    einline


;-------------------------------------------------------------------------------
; errno_t Semaphore_AcquireMultiple(Semaphore* _Nonnull sema, int npermits, const struct timespec* _Nonnull deadline)
; Acquires 'npermits' from the semaphore before the deadline 'deadline' is reached.
; This function blocks the caller if 'deadline' is in the future and the semaphore
; does not have enough permits available.
; This function is guaranteed to not block if 'deadline' is in the past. Returns
; an error code indicating whether the function has timed out waiting for the
; semaphore to become available.
_Semaphore_AcquireMultiple:
    inline
    cargs sa_saved_d7.l, sa_sema.l, sa_npermits.l, sa_deadline.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value >= npermits and consume the permits and return if that's the case. Otherwise wait and retry
.csa_retry:
        move.l  sa_sema(sp), a0
        move.l  sema_value(a0), d0
        cmp.l   sa_npermits(sp), d0
        bge.s   .csa_claim_permits

        ; wait for permits to arrive and then retry
        move.l  sa_deadline(sp), d0
        move.l  d0, -(sp)
        move.l  a0, -(sp)
        jsr     _Semaphore_OnWaitForPermits
        addq.l  #8, sp

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
; errno_t Semaphore_AcquireAll(Semaphore* _Nonnull sema, const struct timespec* _Nonnull deadline, int* _Nonnull pOutPermitCount)
; Acquires all permits from the semaphore before the deadline 'deadline' is reached.
; This function blocks the caller if 'deadline' is in the future and the semaphore
; does not have any permits available.
; This function is guaranteed to not block if 'deadline' is in the past. Returns
; an error code indicating whether the function has timed out waiting for the
; semaphore to become available.
; The number of acquired permits is returned in 'pOutPermitsCount'
_Semaphore_AcquireAll:
    inline
    cargs saa_saved_d7.l, saa_sema.l, saa_deadline.l, saa_out_permits_count_ptr.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value > 0 and consume all permits and return if that's the case. Otherwise wait and retry
.csaa_retry:
        move.l  saa_sema(sp), a0
        move.l  sema_value(a0), d0
        tst.l   d0
        bgt.s   .csaa_claim_permits

        ; wait for permits to arrive and then retry
        move.l  saa_deadline(sp), d0
        move.l  d0, -(sp)
        move.l  a0, -(sp)
        jsr     _Semaphore_OnWaitForPermits
        addq.l  #8, sp

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
; bool Semaphore_TryAcquireMultiple(Semaphore* _Nonnull sema, int npermits)
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
; int Semaphore_TryAcquireAll(Semaphore* _Nonnull sema)
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
