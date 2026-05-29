;
;  __uint64toflt64.s
;  libsc
;
;  Created by Dietmar Planitzer on 5/29/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;
; Based on: <https://stackoverflow.com/questions/64081675/how-to-convert-64-bit-int-to-float-on-32-bit-powerpc>


    xdef __uint64toflt64


;-------------------------------------------------------------------------------
; double __uint64toflt64(uint64_t v)
__uint64toflt64:
    inline
    cargs ui64_h.l, ui64_l.l
        move.l  ui64_h(sp), d0
        move.l  ui64_l(sp), d1


        movem.l d2 - d3, -(sp)
        fmove.x fp2, -(sp)


        move.l  #$43300000, d2      ; magic: 0x1.0p52
        moveq.l #0, d3

        move.l  d3, -(sp)
        move.l  d2, -(sp)
        fmove.d (sp), fp2           ; magic -> fp2


        move.l  d0, -(sp)
        move.l  d2, -(sp) 
        fmove.d (sp), fp0           ; ui64_h -> fp0
        fsub    fp2, fp0            ; ui64_h - magic -> fp0

        move.l  d1, -(sp)
        move.l  d2, -(sp) 
        fmove.d (sp), fp1           ; ui64_l -> fp1
        fsub    fp2, fp1            ; ui64_l - magic -> fp1


        ;fmove.d #4294967296.0, fp2  ; using fscale instead
        ;fmul    fp2, fp0            ; ui64h << 32
        
        fscale  #32, fp0            ; ui64h << 32
        fadd    fp1, fp0            ; r = (ui64_h << 32) + ui64_l


        add.l   #(3*8), sp
        fmove.x (sp)+, fp2
        movem.l (sp)+, d2 - d3
        rts
    einline
