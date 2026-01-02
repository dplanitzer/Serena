;
;  arch/m68k/udiv.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 1/1/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _udiv
    xdef _uldiv


;-------------------------------------------------------------------------------
; udiv_t udiv(unsigned int x, unsigned int y)
; uldiv_t uldiv(unsigned long x, unsigned long y)
_udiv:
_uldiv:
    cargs   dividend.l, divisor.l

    move.l  dividend(sp), d0
    move.l  divisor(sp), d1
    divul.l d1, d1:d0       ; d1/d0 = remainder:quotient
    rts
