;
;  __mulint64.s
;  libsc
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    xdef __mulint64_020
    xdef __mulint64_060
    xdef __mulsint64_020
    xdef __mulsint64_060
    xdef __muluint64_020
    xdef __muluint64_060


;-------------------------------------------------------------------------------
; __mulsint64_020(x: d0/d1, y: d2/d3) -> d0/d1
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
; 64bit signed multiplication
__mulint64_020:
__mulsint64_020:
    inline
    cargs #(4 + 4), mul20_si64_xh.l, mul20_si64_xl.l, mul20_si64_yh.l, mul20_si64_yl.l
        move.l  d2, -(sp)

        move.l  mul20_si64_xh(sp), d0
        move.l  mul20_si64_yh(sp), d1
        bne.s   .do_full_mul
        tst.l   d0
        bne.s   .do_full_mul
        moveq.l #0, d2
        bra.s   .do_32_to_64_mul

.do_full_mul:
        move.l  mul20_si64_yl(sp), d2
        muls.l  d0, d2                  ; d2 = x_h*y_l

        move.l  mul20_si64_xl(sp), d0
        muls.l  d1, d0                  ; d0 = x_l*y_h

        add.l   d0, d2                  ; d2 = (x_h*y_l + x_l*y_h)*2^32

.do_32_to_64_mul:
        move.l  mul20_si64_xl(sp), d0
        move.l  mul20_si64_yl(sp), d1
        mulu.l  d0, d0:d1               ; d0:d1 = x_l*y_l

        add.l   d2, d0                  ; d0:_ = x_l*y_l + (x_h*y_l + x_l*y_h)*2^32

        move.l  (sp)+, d2
        rts
    einline


;-------------------------------------------------------------------------------
; __mulsint64_060(x: d0/d1, y: d2/d3) -> d0/d1
; 64bit signed multiplication
__mulint64_060:
__mulsint64_060:
    inline
    cargs #(4 + 4), mul60_si64_xh.l, mul60_si64_xl.l, mul60_si64_yh.l, mul60_si64_yl.l
        move.l  d2, -(sp)

        move.l  mul60_si64_xh(sp), d0
        move.l  mul60_si64_yh(sp), d1
        bne.s   .do_full_mul
        tst.l   d0
        bne.s   .do_full_mul
        moveq.l #0, d2
        bra.s   .do_32_to_64_mul

.do_full_mul:
        move.l  mul60_si64_yl(sp), d2
        muls.l  d0, d2                  ; d2 = x_h*y_l

        move.l  mul60_si64_xl(sp), d0
        muls.l  d1, d0                  ; d0 = x_l*y_h

        add.l   d0, d2                  ; d2 = (x_h*y_l + x_l*y_h)*2^32

.do_32_to_64_mul:
        move.l  mul60_si64_xl(sp), d0
        move.l  mul60_si64_yl(sp), d1
        bsr.s   __mul_ui32_64

        add.l   d2, d0                  ; d0:_ = x_l*y_l + (x_h*y_l + x_l*y_h)*2^32

        move.l  (sp)+, d2
        rts
    einline


;-------------------------------------------------------------------------------
; __muluint64(x: d0/d1, y: d2/d3) -> d0/d1
; 64bit unsigned multiplication
__muluint64_020:
    inline
    cargs #(4 + 4), mul20_ui64_xh.l, mul20_ui64_xl.l, mul20_ui64_yh.l, mul20_ui64_yl.l
        move.l  d2, -(sp)

        move.l  mul20_ui64_xh(sp), d0
        move.l  mul20_ui64_yh(sp), d1
        bne.s   .do_full_mul
        tst.l   d0
        bne.s   .do_full_mul
        moveq.l #0, d2
        bra.s   .do_32_to_64_mul

.do_full_mul:
        move.l  mul20_ui64_yl(sp), d2
        mulu.l  d0, d2                  ; d2 = x_h*y_l

        move.l  mul20_ui64_xl(sp), d0
        mulu.l  d1, d0                  ; d0 = x_l*y_h

        add.l   d0, d2                  ; d2 = (x_h*y_l + x_l*y_h)*2^32

.do_32_to_64_mul:
        move.l  mul20_ui64_xl(sp), d0
        move.l  mul20_ui64_yl(sp), d1
        mulu.l  d0, d0:d1               ; d0:d1 = x_l*y_l

        add.l   d2, d0                  ; d0:_ = x_l*y_l + (x_h*y_l + x_l*y_h)*2^32

        move.l  (sp)+, d2
        rts
    einline


;-------------------------------------------------------------------------------
; __muluint64(x: d0/d1, y: d2/d3) -> d0/d1
; 64bit unsigned multiplication
__muluint64_060:
    inline
    cargs #(4 + 4), mul60_ui64_xh.l, mul60_ui64_xl.l, mul60_ui64_yh.l, mul60_ui64_yl.l
        move.l  d2, -(sp)

        move.l  mul60_ui64_xh(sp), d0
        move.l  mul60_ui64_yh(sp), d1
        bne.s   .do_full_mul
        tst.l   d0
        bne.s   .do_full_mul
        moveq.l #0, d2
        bra.s   .do_32_to_64_mul

.do_full_mul:
        move.l  mul60_ui64_yl(sp), d2
        mulu.l  d0, d2                  ; d2 = x_h*y_l

        move.l  mul60_ui64_xl(sp), d0
        mulu.l  d1, d0                  ; d0 = x_l*y_h

        add.l   d0, d2                  ; d2 = (x_h*y_l + x_l*y_h)*2^32

.do_32_to_64_mul:
        move.l  mul60_ui64_xl(sp), d0
        move.l  mul60_ui64_yl(sp), d1
        bsr.s   __mul_ui32_64

        add.l   d2, d0                  ; d0:_ = x_l*y_l + (x_h*y_l + x_l*y_h)*2^32

        move.l  (sp)+, d2
        rts
    einline


;-------------------------------------------------------------------------------
; __mul_ui32_64(A: d0, B: d1) -> d0:d1
; 32bit by 32bit unsigned multiplication with a 64bit result
;
; from the book:
; Assembly Language and Systems Programming for the M68000 Family, Second Edition
; by William Ford & William Topp
; Jones and Bartlett Publishers
; Pages 338, 339
__mul_ui32_64:
    movem.l d2-d4, -(sp)

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

    movem.l (sp)+, d2-d4
    rts
