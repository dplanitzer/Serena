;
;  UInt64_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


;-------------------------------------------------------------------------------
; 64bit unsigned arithmetic
;-------------------------------------------------------------------------------


    xdef __rshuint64
    xdef __lshuint64
    xdef __ui32_64_mul


;-------------------------------------------------------------------------------
; UInt64 __rshuint64(UInt64 x, Int32 s)
; 64bit unsigned shift right
__rshuint64:
    inline
    cargs rui64_saved_d7.l, rui64_xh.l, rui64_xl.l, rui64_s.l
        movem.l d7, -(sp)
        move.l  rui64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  rui64_xh(sp), d0
        move.l  rui64_xl(sp), d1
.L2:
        lsr.l   #1, d0                  ; shift right by one bit per loop iteration
        roxr.l  d1
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; UInt64 __lshuint64(UInt64 x, Int32 s)
; 64bit unsigned shift left
__lshuint64:
    inline
    cargs lui64_saved_d7.l, lui64_xh.l, lui64_xl.l, lui64_s.l
        movem.l d7, -(sp)
        move.l  lui64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  lui64_xh(sp), d0
        move.l  lui64_xl(sp), d1
.L2:
        lsl.l   #1, d1                  ; shift left by one bit per loop iteration
        roxl.l  d0
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
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
