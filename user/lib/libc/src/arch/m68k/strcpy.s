;
;  arch/m68k/strcpy.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 2/28/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _strcpy


;-------------------------------------------------------------------------------
; char * _Nonnull strcpy(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
_strcpy:
    cargs dst.l, src.l

    move.l  src(sp), a0
    move.l  dst(sp), a1
    move.l  a1, d0

.1:
    move.b  (a0)+, d1
    beq.s   .2
    move.b  d1, (a1)+
    bra.s   .1

.2:
    move.b  d1, (a1)
    rts
