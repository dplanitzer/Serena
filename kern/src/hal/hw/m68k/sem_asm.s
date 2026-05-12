;
;  sem_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/10/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "cpu.i"

    xref _sem_on_wait
    xref _sem_on_timedwait
    xref _sem_wakeup

    xdef _sem_post
    xdef _sem_wait
    xdef _sem_timedwait
    xdef _sem_trywait


    clrso
sem_value              so.l    1
sem_wait_queue_first   so.l    1
sem_wait_queue_last    so.l    1
sem_SIZEOF             so


;-------------------------------------------------------------------------------
; void sem_post(sem_t* _Nonnull sem)
_sem_post:
    inline
    cargs sp_sem.l
        move.l  sp_sem(sp), a0

        DISABLE_PREEMPTION d0
        addq.l  #1, sem_value(a0)

        ; move all the waiters back to the ready queue
        move.l  d0, -(sp)
        move.l  a0, -(sp)
        jsr     _sem_wakeup
        addq.l  #4, sp
        move.l  (sp)+, d0

        RESTORE_PREEMPTION d0
        rts
    einline


;-------------------------------------------------------------------------------
;void sem_wait(sem_t* _Nonnull sem)
; Acquires a permit from the semaphore.
_sem_wait:
    inline
    cargs sw_saved_d7.l, sw_sem.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value >= 1 and consume the permits and return if that's the case. Otherwise wait and retry
.retry:
        move.l  sw_sem(sp), a0
        move.l  sem_value(a0), d0
        cmp.l   #1, d0
        bge.s   .claim_permit

        ; wait for permits to arrive and then retry
        move.l  a0, -(sp)
        jsr     _sem_on_wait
        addq.l  #4, sp
        bra.s   .retry

.claim_permit:
        subq.l  #1, sem_value(a0)
        RESTORE_PREEMPTION d7
        move.l  (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; errno_t sem_timedwait(sem_t* _Nonnull sem, const nanotime_t* _Nonnull deadline)
; Acquires a permit from the semaphore before the deadline 'deadline' is reached.
; This function blocks the caller if 'deadline' is in the future and the semaphore
; does not have enough permits available.
; This function is guaranteed to not block if 'deadline' is in the past. Returns
; an error code indicating whether the function has timed out waiting for the
; semaphore to become available.
_sem_timedwait:
    inline
    cargs swt_saved_d7.l, swt_sem.l, swt_deadline.l
        move.l  d7, -(sp)
        DISABLE_PREEMPTION d7

        ; check whether value >= 1 and consume the permits and return if that's the case. Otherwise wait and retry
.retry:
        move.l  swt_sem(sp), a0
        move.l  sem_value(a0), d0
        cmp.l   #1, d0
        bge.s   .claim_permit

        ; wait for permits to arrive and then retry
        move.l  swt_deadline(sp), d0
        move.l  d0, -(sp)
        move.l  a0, -(sp)
        jsr     _sem_on_timedwait
        addq.l  #8, sp

        ; give up if the sem_on_timedwait came back with an error
        tst.l   d0
        bne.s   .done   ; d0 has the error code

        bra.s   .retry

.claim_permit:
        subq.l  #1, sem_value(a0)
        moveq.l #0, d0

.done:
        RESTORE_PREEMPTION d7
        move.l  (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; bool sem_trywait(sem_t* _Nonnull sem)
; Tries to acquire one permit from the semaphore. Returns true if the acquisition
; was successful and false otherwise. This function does not block.
_sem_trywait:
    inline
    cargs stw_saved_d7.l, stw_sem.l
        move.l  d7, -(sp)
        move.l  stw_sem(sp), a0

        DISABLE_PREEMPTION d7

        move.l  sem_value(a0), d0
        cmp.l   #1, d0
        bge.s   .claim_permit

        moveq.l #0, d0
        bra.s   .done

.claim_permit:
        subq.l  #1, sem_value(a0)
        moveq.l #-1, d0

.done:
        RESTORE_PREEMPTION d7
        move.l  (sp)+, d7
        rts
    einline
