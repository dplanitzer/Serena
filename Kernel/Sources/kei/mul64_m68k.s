;
;  mul64_m68k.s
;  crt
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    xdef __mulint64_020
    xdef __mulint64_060
    xdef __ui32_64_mul


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


;-------------------------------------------------------------------------------
; __ui32_64_mul(A: d0, B: d1) -> d0:d1
; 32bit by 32bit unsigned multiplication with a 64bit result
;
; from the book:
; Assembly Language and Systems Programming for the M68000 Family, Second Edition
; by William Ford & William Topp
; Jones and Bartlett Publishers
; Pages 338, 339
__ui32_64_mul:
    movem.l d2 - d4, -(sp)

    move.l  d1, d2              ; copy a to d2 & d3
    move.l  d1, d3
    move.l  d0, d4              ; copy b to d4
    swap    d3                  ; d3 = al || ah
    swap    d4                  ; d4 = bl || bh
    mulu    d0, d1              ; d1 = al * bl
    mulu    d3, d0              ; d0 = bl * ah
    mulu    d4, d2              ; d2 = bh * al
    mulu    d4, d3              ; d3 = bh * ah

    ; add up the partial products
    moveq   #0, d4              ; used with adds to add the carry
    swap    d1                  ; d1 = low(al:bl) || high(al:bl)
    add.w   d0, d1              ; d1 = low(al:bl) || high(al:bl) + low(bl:ah)
    addx.l  d4, d3              ; add carry from previous add
    add.w   d2, d1              ; d1 = low(al:bl) || high(al:bl) + low(bl:ah) + low(bh:al)
    addx.l  d4, d3              ; add carry from previous add
    swap    d1                  ; put d1 into its final form
    clr.w   d0                  ; d0 = high(bl:ah) || 0
    swap    d0                  ; d0 = 0 || high(bl:ah)
    clr.w   d2
    swap    d2                  ; d2 = 0 || high(bh:al)
    add.l   d2, d0              ; carry is stored in msg of d0
    add.l   d3, d0              ; d0 = high(ah:bh) + carry || low(bh:ah) + high(bl:ah) + high(bh:al)

    movem.l (sp)+, d2 - d4
    rts
