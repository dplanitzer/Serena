;
;  arch/m68k/strcmp.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 2/28/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _strcmp


;-------------------------------------------------------------------------------
; int strcmp(const char * _Nonnull lhs, const char * _Nonnull rhs)
_strcmp:
    cargs lhs.l, rhs.l

    move.l  lhs(sp), a0
    move.l  rhs(sp), a1

.1:
    move.b  (a0)+, d0
    move.b  (a1)+, d1
    cmp.b   d1, d0
    bne.s   .2
    tst.b   d0
    beq.s   .4
    bra.s   .1

.2:
    blo.s   .3      ; (*((unsigned char*)lhs) < *((unsigned char*)rhs)) ? -1 : 1
    moveq.l #1, d0
    rts

.3:
    moveq.l #-1, d0
    rts

.4:
    moveq.l #0, d0
    rts
