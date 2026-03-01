;
;  arch/m68k/strchr.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 2/28/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _strchr


;-------------------------------------------------------------------------------
; char * _Nullable strchr(const char * _Nonnull str, int ch)
_strchr:
    cargs str.l, ch.l

    move.l  str(sp), a0
    move.l  ch(sp), d1

.1:
    move.b  (a0)+, d0
    beq.s   .2
    cmp.b   d1, d0
    bne.s   .1
    ; found 'ch' in the string -> fall through

.3:
    subq.l  #1, a0
    move.l  a0, d0
    rts

.2:
    tst.b   d1      ; was user specifically looking for the trailing NUL? If so -> found it; otherwise 'ch' doesn't appear in the string
    beq.s   .3

    moveq.l #0, d0
    rts
