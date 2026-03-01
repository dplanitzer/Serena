;
;  arch/m68k/strlen.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 2/28/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _strlen


;-------------------------------------------------------------------------------
; size_t strlen(const char * _Nonnull str)
_strlen:
    cargs str.l

    move.l  str(sp), a0
    move.l  a0, d1

.1:
    tst.b   (a0)+
    bne.s   .1

    move.l  a0, d0
    sub.l   d1, d0
    subq.l  #1, d0
    rts
