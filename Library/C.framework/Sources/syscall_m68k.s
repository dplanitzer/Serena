;
;  syscall_m68k.s
;  Apollo
;
;  Created by Dietmar Planitzer on 9/2/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include "syscalldef.i"

    xdef ___syscall


;-------------------------------------------------------------------------------
; errno_t __syscall(int scno, ...)
___syscall:
    lea.l  4(sp), a0
    SYSCALL
    rts
