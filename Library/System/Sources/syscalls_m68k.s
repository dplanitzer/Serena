;
;  syscalls_m68k.s
;  Apollo
;
;  Created by Dietmar Planitzer on 7/9/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include <syscalls.i>

    xdef _print
    xdef _dispatchAsync
    xdef _sleep


;-------------------------------------------------------------------------------
; void print(Character* _Nonnull pString)
_print:
    cargs prt_string_ptr.l
    move.l  prt_string_ptr(sp), a1
    SYSCALL SC_print
    rts


;-------------------------------------------------------------------------------
; void dispatchAsync(void* _Nonnull pUserClosure)
_dispatchAsync:
    cargs dspasync_closure_ptr.l
    move.l  dspasync_closure_ptr(sp), a1
    SYSCALL SC_dispatchAsync
    rts


;-------------------------------------------------------------------------------
; void sleep(Int seconds, Int nanoseconds)
_sleep:
    cargs slp_saved_d2.l, slp_seconds.l, slp_nanoseconds.l
    move.l  d2, -(sp)
    move.l  slp_seconds(sp), d1
    move.l  slp_nanoseconds(sp), d2
    SYSCALL SC_sleep
    move.l  (sp)+, d2
    rts
