;
;  __sint64toflt64.s
;  libsc
;
;  Created by Dietmar Planitzer on 5/29/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;
; Based on: <https://stackoverflow.com/questions/64081675/how-to-convert-64-bit-int-to-float-on-32-bit-powerpc>


    xdef __sint64toflt64

    xref __uint64toflt64


;-------------------------------------------------------------------------------
; double __sint64toflt64(long long v)
__sint64toflt64:
    inline
    cargs i64_h.l, i64_l.l
        move.l  i64_l(sp), d1
        move.l  i64_h(sp), d0

        bmi.s   .negative
        jmp     __uint64toflt64(pc)     ; chain to the unsigned version
        ; NOT REACHED

.negative:
        neg.l   d1
        negx.l  d0
        
        move.l  d1, -(sp)
        move.l  d0, -(sp)
        jsr __uint64toflt64(pc)
        addq.l  #8, sp

        fneg    fp0

        rts
    einline
