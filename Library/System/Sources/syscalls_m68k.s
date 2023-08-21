;
;  syscalls_m68k.s
;  Apollo
;
;  Created by Dietmar Planitzer on 7/9/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include <syscalls.i>

    xdef _write
    xdef _dispatch_async
    xdef _sleep
    xdef _alloc_address_space
    xdef _exit


;-------------------------------------------------------------------------------
; ErrorCode write(Character* _Nonnull pString)
_write:
    cargs prt_string_ptr.l
    move.l  prt_string_ptr(sp), d1
    SYSCALL SC_write
    rts


;-------------------------------------------------------------------------------
; ErrorCode dispatch_async(void* _Nonnull pUserClosure)
_dispatch_async:
    cargs dspasync_closure_ptr.l
    move.l  dspasync_closure_ptr(sp), d1
    SYSCALL SC_dispatch_async
    rts


;-------------------------------------------------------------------------------
; ErrorCode sleep(Int seconds, Int nanoseconds)
_sleep:
    cargs slp_saved_d2.l, slp_seconds.l, slp_nanoseconds.l
    move.l  d2, -(sp)
    move.l  slp_seconds(sp), d1
    move.l  slp_nanoseconds(sp), d2
    SYSCALL SC_sleep
    move.l  (sp)+, d2
    rts


;-------------------------------------------------------------------------------
; ErrorCode alloc_address_space(Int nbytes, Byte* _Nullable * _Nonnull pOutMemPtr)
_alloc_address_space:
    cargs aas_nbytes.l, aas_pOutMemPtr.l
    move.l  aas_nbytes(sp), d1
    SYSCALL SC_alloc_address_space
    move.l  aas_pOutMemPtr(sp), a0
    move.l  d1, (a0)
    rts


;-------------------------------------------------------------------------------
; _Noreturn exit(Int status)
_exit:
    cargs exit_status.l
    move.l  exit_status(sp), d1
    SYSCALL SC_exit
    rts
