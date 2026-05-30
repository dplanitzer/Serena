;
;  __flt64tosint64.s
;  libsc
;
;  Created by Dietmar Planitzer on 5/29/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.


    xdef __flt64tosint64

    xref __flt64touint64


;-------------------------------------------------------------------------------
; long long __flt64tosint64(double v)
__flt64tosint64:
    inline
    cargs v64_h.l, v64_l.l
        ftst.d  4(sp)
        bmi.s   .negative
        jmp     __flt64touint64(pc)     ; chain to the unsigned version
        ; NOT REACHED

.negative:
        fneg.d  4(sp), fp0
        fmove.d fp0, -(sp)
        jsr     __flt64touint64(pc)
        addq.l  #8, sp
        
        neg.l   d1
        negx.l  d0

        rts
    einline
