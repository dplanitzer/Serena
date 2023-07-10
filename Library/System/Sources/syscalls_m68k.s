;
;  syscalls_m68k.s
;  Apollo
;
;  Created by Dietmar Planitzer on 7/9/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include <syscalls.i>

    xdef _print
    xdef _sleep


;-------------------------------------------------------------------------------
; void print(Character* _Nonnull pString)
_print:
    cargs prt_string_ptr.l
    move.l  prt_string_ptr(sp), a1
    SYSCALL SC_PRINT
    rts


;-------------------------------------------------------------------------------
; void sleep(Int seconds, Int nanoseconds)
_sleep:
    cargs slp_saved_d2.l, slp_seconds.l, slp_nanoseconds.l
    move.l  d2, -(sp)
    move.l  slp_seconds(sp), d1
    move.l  slp_nanoseconds(sp), d2
    SYSCALL SC_SLEEP
    move.l  (sp)+, d2
    rts
