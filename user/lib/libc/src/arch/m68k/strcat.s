;
;  arch/m68k/strcat.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 2/28/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _strcat


;-------------------------------------------------------------------------------
; char * _Nonnull strcat(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
_strcat:
    cargs dst.l, src.l

    move.l  src(sp), a0
    move.l  dst(sp), a1
    move.l  a1, d0

    tst.b   (a0)
    beq.s   .4


    ; 'dst' may point to a string - skip it
.1:
    tst.b   (a1)+
    bne.s   .1
    subq.l  #1, a1


    ; append a copy of 'src' to the destination
.2:
    move.b  (a0)+, d1
    beq.s   .3
    move.b  d1, (a1)+
    bra.s   .2

.3:
    move.b  d1, (a1)

.4:
    rts
