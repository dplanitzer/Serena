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


    xdef __addsint64
    xdef __adduint64
    xdef __subsint64
    xdef __subuint64
    xdef __negsint64
    xdef __abssint64
    xdef __rshsint64
    xdef __lshsint64
    xdef __mulint64
    xdef __mulint64_020


;-------------------------------------------------------------------------------
; Int64 __addsint64(Int64 x, Int64 y)
; 64bit signed add
;
; UInt64 __adduint64(UInt64 x, UInt64 y)
; 64bit unsigned add
__addsint64
__adduint64:
    inline
    cargs addi64_saved_d2.l, addsi64_saved_d3.l, addsi64_xh.l, addsi64_xl.l, addsi64_yh.l, addsi64_yl.l
        movem.l d2 - d3, -(sp)
        movem.l addsi64_xh(sp), d0 - d1
        movem.l addsi64_yh(sp), d2 - d3
        add.l   d3, d1
        addx.l  d2, d0
        movem.l (sp)+, d2 - d3
        rts
    einline


;-------------------------------------------------------------------------------
; Int64 __subsint64(Int64 x, Int64 y)
; 64bit signed subtract
;
; UInt64 __subuint64(UInt64 x, UInt64 y)
; 64bit unsigned subtract
__subsint64:
__subuint64:
    inline
    cargs subi64_saved_d2.l, subsi64_saved_d3.l, subsi64_xh.l, subsi64_xl.l, subsi64_yh.l, subsi64_yl.l
        movem.l d2 - d3, -(sp)
        movem.l subsi64_xh(sp), d0 - d1
        movem.l subsi64_yh(sp), d2 - d3
        sub.l   d3, d1
        subx.l  d2, d0
        movem.l (sp)+, d2 - d3
        rts
    einline


;-------------------------------------------------------------------------------
; Int64 __negsint64(Int64 x)
; 64bit signed negate
__negsint64:
    inline
    cargs negsi64_xh.l, negsi64_xl.l
        movem.l negsi64_xh(sp), d0 - d1
        neg.l   d1
        negx.l  d0
        rts
    einline


;-------------------------------------------------------------------------------
; Int64 __abssint64(Int64 x)
; 64bit absolute value
__abssint64:
    inline
    cargs abssi64_xh.l, abssi64_xl.l
        movem.l abssi64_xh(sp), d0 - d1
        tst.l   d0
        bpl.s   .L1
        neg.l   d1
        negx.l  d0
.L1:
        rts
    einline


;-------------------------------------------------------------------------------
; Int64 __rshsint64(Int64 x, Int32 s)
; 64bit signed shift right
__rshsint64:
    inline
    cargs rsi64_saved_d7.l, rsi64_xh.l, rsi64_xl.l, rsi64_s.l
        movem.l d7, -(sp)
        move.l  rsi64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  rsi64_xh(sp), d0
        move.l  rsi64_xl(sp), d1
.L2:
        asr.l   #1, d0                  ; shift right by one bit per loop iteration
        roxr.l  d1
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; Int64 __lshsint64(Int64 x, Int32 s)
; 64bit signed shift left
__lshsint64:
    inline
    cargs lsi64_saved_d7.l, lsi64_xl.l, lsi64_xh.l, lsi64_s.l
        move.l  lsi64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  lsi64_xh(sp), d0
        move.l  lsi64_xl(sp), d1
.L2:
        asl.l   #1, d0                  ; shift left by one bit per loop iteration
        roxl.l  d1
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
        rts
    einline


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
__mulint64:
__mulint64_020:
    inline
    cargs muli64_saved_d2.l, muli64_xh.l, muli64_xl.l, muli64_yh.l, muli64_yl.l
        move.l  d2, -(sp)

        move.l  muli64_xh(sp), d0
        bne.s   .L1                 ; only do the first high product if x_h != 0
        moveq   #0, d2
        bra.s   .L2
.L1:
        move.l  muli64_yl(sp), d1
    ifd TARGET_CPU_68030
        mulu.l  d1, d0
    else
        jsr     __ui32_mul
    endif
        move.l  d0, d2              ; d2 = x_h*y_l

.L2:
        move.l  muli64_yh(sp), d1
        bne.s   .L3                 ; only do the second high product if y_h != 0
        moveq   #0, d0
        bra.s   .L4
.L3:
        move.l  muli64_xl(sp), d0
    ifd TARGET_CPU_68030
        mulu.l  d1, d0              ; d0 = x_l*y_h
    else
        jsr     __ui32_mul          ; d0 = x_l*y_h
    endif

.L4:
        add.l   d0, d2              ; d2 = (x_h*y_l + x_l*y_h)*2^32

        move.l  muli64_xl(sp), d0
        move.l  muli64_yl(sp), d1
        jsr     __ui32_64_mul       ; d0:d1 = x_l*y_l

        add.l   d2, d1              ; _:d1 = x_l*y_l + (x_h*y_l + x_l*y_h)*2^32

        move.l  (sp)+, d2
        rts
    einline
