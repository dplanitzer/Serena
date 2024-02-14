;
;  syscall_m68k.s
;  libsystem
;
;  Created by Dietmar Planitzer on 9/2/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include <System/asm/syscall.i>

    xdef __syscall


;-------------------------------------------------------------------------------
; intptr_t _syscall(int scno, ...)
__syscall:
    lea.l  4(sp), a0
    SYSCALL
    rts
