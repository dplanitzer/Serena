;
;  __flt64touint64.s
;  libsc
;
;  Created by Dietmar Planitzer on 5/29/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.


    xdef __flt64touint64


;-------------------------------------------------------------------------------
; unsigned long long __flt64touint64(double v)
__flt64touint64:
    inline
    cargs v64_h.l, v64_l.l
        fmove.d #4503599627370496.0, fp0    ; 0x1.0p52
        fadd.d  4(sp), fp0
        fmove.d fp0, -(sp)

        move.l  (sp)+, d0
        move.l  (sp)+, d1
        and.l   #$FFFFF, d0                 ; result is the low 52 bits of the adjusted double value

        rts
    einline
