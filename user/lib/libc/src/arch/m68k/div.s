;
;  arch/m68k/div.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 1/1/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _div
    xdef _ldiv


;-------------------------------------------------------------------------------
; div_t div(int x, int y)
; ldiv_t ldiv(long x, long y)
_div:
_ldiv:
    cargs   dividend.l, divisor.l

    move.l  dividend(sp), d0
    move.l  divisor(sp), d1
    divsl.l d1, d1:d0       ; d1/d0 = remainder:quotient
    rts
