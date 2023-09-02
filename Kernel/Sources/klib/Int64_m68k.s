;
;  Int64_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


;-------------------------------------------------------------------------------
; 64bit signed arithmetic
;-------------------------------------------------------------------------------


    xdef __mulint64_020
    xdef __mulint64_060


;-------------------------------------------------------------------------------
; __mulint64(x: d0/d1, y: d2/d3) -> d0/d1
; 64bit signed multiplication
;
; x, y: 64-bit integer
; x_h/x_l: higher/lower 32 bits of x
; y_h/y_l: higher/lower 32 bits of y
;
; x*y  = ((x_h*2^32 + x_l)*(y_h*2^32 + y_l)) mod 2^64
;      = (x_h*y_h*2^64 + x_l*y_l + x_h*y_l*2^32 + x_l*y_h*2^32) mod 2^64
;      = x_l*y_l + (x_h*y_l + x_l*y_h)*2^32
;
; see <https://stackoverflow.com/questions/19601852/assembly-64-bit-multiplication-with-32-bit-registers>
;
__mulint64_020:
__mulint64_060:
    inline
    cargs muli64_saved_d2.l, muli64_xh.l, muli64_xl.l, muli64_yh.l, muli64_yl.l
        move.l  d2, -(sp)

        move.l  muli64_xh(sp), d0
        bne.s   .L1                 ; only do the first high product if x_h != 0
        moveq   #0, d2
        bra.s   .L2
.L1:
        move.l  muli64_yl(sp), d1
        mulu.l  d1, d0
        move.l  d0, d2              ; d2 = x_h*y_l

.L2:
        move.l  muli64_yh(sp), d1
        bne.s   .L3                 ; only do the second high product if y_h != 0
        moveq   #0, d0
        bra.s   .L4
.L3:
        move.l  muli64_xl(sp), d0
        mulu.l  d1, d0              ; d0 = x_l*y_h

.L4:
        add.l   d0, d2              ; d2 = (x_h*y_l + x_l*y_h)*2^32

        move.l  muli64_xl(sp), d0
        move.l  muli64_yl(sp), d1
        jsr     __ui32_64_mul       ; d0:d1 = x_l*y_l

        add.l   d2, d1              ; _:d1 = x_l*y_l + (x_h*y_l + x_l*y_h)*2^32

        move.l  (sp)+, d2
        rts
    einline
