;
;  Int32_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


;-------------------------------------------------------------------------------
; 32bit arithmetic routines
;-------------------------------------------------------------------------------

    xdef __ldivs
    xdef __lmods
    xdef __ldivu
    xdef __lmodu
    xdef __lmuls
    xdef __lmulu
    xdef __l3muls
    xdef __l3mulu
    ifnd TARGET_CPU_68030
    xdef __ui32_mul
    endif
    xdef __ui32_64_mul


;-------------------------------------------------------------------------------
; Int32 _ldivs(Int32, Int32)
__ldivs:
    inline
    cargs ldivs_dividend.l, ldivs_divisor
    ifd TARGET_CPU_68030
        move.l  ldivs_dividend(sp), d1
        divsl.l ldivs_divisor(sp), d1:d0
        rts
    else
        move.l  ldivs_dividend(sp), d0
        move.l  ldivs_divisor(sp), d1
        jmp     i32_divmod
    endif
    einline


;-------------------------------------------------------------------------------
; Int32 _lmods(Int32, Int32)
__lmods:
    inline
    cargs lmods_dividend.l, lmods_divisor
    ifd TARGET_CPU_68030
        move.l  lmods_dividend(sp), d0
        divsl.l lmods_divisor(sp), d0:d1
        rts
    else
        move.l  lmods_dividend(sp), d0
        move.l  lmods_divisor(sp), d1
        jsr     i32_divmod
        move.l  d1, d0
        rts
    endif
    einline


;-------------------------------------------------------------------------------
; UInt32 _ldivu(UInt32, UInt32)
__ldivu:
    inline
    cargs ldivu_dividend.l, ldivu_divisor
    ifd TARGET_CPU_68030
        move.l  ldivu_dividend(sp), d1
        divul.l ldivu_divisor(sp), d1:d0
        rts
    else
        move.l  ldivu_dividend(sp), d0
        move.l  ldivu_divisor(sp), d1
        jmp     ui32_divmod
    endif
    einline


;-------------------------------------------------------------------------------
; UInt32 _lmods(UInt32, UInt32)
__lmodu:
    inline
    cargs lmodu_dividend.l, lmodu_divisor
    ifd TARGET_CPU_68030
        move.l  lmodu_dividend(sp), d0
        divul.l lmodu_divisor(sp), d0:d1
        rts
    else
        move.l  lmodu_dividend(sp), d0
        move.l  lmodu_divisor(sp), d1
        jsr     ui32_divmod
        move.l  d1, d0
        rts
    endif
    einline




;-------------------------------------------------------------------------------
; Int32  __lmuls(Int32, Int32)
; 32bit by 32bit signed multiplication with 32bit result
__lmuls:
    inline
    cargs lmuls_a.l, lmuls_b.l
    ifd TARGET_CPU_68030
        move.l  lmuls_b(sp), d0
        muls.l  lmuls_a(sp), d0
        rts
    else
        move.l  lmuls_a(sp), d0
        move.l  lmuls_b(sp), d1
        jmp     __ui32_mul
    endif
    einline


;-------------------------------------------------------------------------------
; UInt32 __lmulu(UInt32, UInt32)
; 32bit by 32bit unsigned multiplication with 32bit result
__lmulu:
    inline
    cargs lmul_a.l, lmul_b.l
    ifd TARGET_CPU_68030
        move.l  lmul_b(sp), d0
        mulu.l  lmul_a(sp), d0
        rts
    else
        move.l  lmul_a(sp), d0
        move.l  lmul_b(sp), d1
        jmp     __ui32_mul
    endif
    einline


;-------------------------------------------------------------------------------
; Int64  __l3muls(Int32, Int32)
; 32bit by 32bit signed multiplication with 64bit result
__l3muls:
    inline
    cargs l3muls_a.l, l3muls_b.l
        move.l  l3muls_a(sp), d0
        move.l  l3muls_b(sp), d1
        jmp     __ui32_64_mul
    einline


;-------------------------------------------------------------------------------
; UInt64 __l3mulu(UInt32, UInt32)
; 32bit by 32bit unsigned multiplication with 64bit result
__l3mulu:
    inline
    cargs l3mul_a.l, l3mul_b.l
        move.l  l3mul_a(sp), d0
        move.l  l3mul_b(sp), d1
        jmp     __ui32_64_mul
    einline




    ifnd TARGET_CPU_68030

;-------------------------------------------------------------------------------
; __ui32_mul(A: d0, B: d1) -> d0
; 32bit by 32bit unsigned multiplication with a 32bit result
;
; based on the 32x32->64 bit unsigned multiply routine from the book:
; Assembly Language and Systems Programming for the M68000 Family, Second Edition
; by William Ford & William Topp
; Jones and Bartlett Publishers
; Pages 338, 339
__ui32_mul:
    inline
        movem.l d2 - d3, -(sp)

        move.l  d0, d2      ; d0=AhAl
        swap    d2          ; d2=AlAh

        move.l  d1, d3      ; d1=BhBl
        swap    d3          ; d3=BlBh

        mulu.w  d1, d0      ; d0=Al*Bl
        mulu.w  d1, d2      ; d2=Ah*Bl
        mulu.w  d0, d3      ; d3=Al*Bh

        add.w   d3, d2      ; compute result high bits
        swap.w  d2
        clr.w   d2
        add.l   d2, d0      ; add in result low bits

        movem.l (sp)+, d2 - d3
        rts
    einline

    endif

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




    ifnd TARGET_CPU_68030
    
;-------------------------------------------------------------------------------
; i32_divmod(numerator: d0, denominator: d1) -> (quotient: d0, remainder: d1)
; 32bit signed division and modulos. The actual work is done by the unsigned
; divide routine ui32_divmod. This function here fixes up the sign of the result
; based on the sign of the numerator/denominator.
i32_divmod:
    inline
        tst.l   d0
        bpl.s   .NumPos
        neg.l   d0
        tst.l   d1
        bpl.s   .DenomPos
        neg.l   d1
        bsr.s   ui32_divmod
        neg.l   d1
        rts

.DenomPos:
        bsr.s   ui32_divmod
        neg.l   d0
        neg.l   d1
        rts

.NumPos:
        tst.l   d1
        bpl.s   ui32_divmod
        neg.l   d1
        bsr.s   ui32_divmod
        neg.l   d0
        rts
    einline


;-------------------------------------------------------------------------------
; ui32_divmod(dividend: d0, divisor: d1) -> (quotient: d0, remainder: d1)
; 32bit unsigned division and modulos
;
; based on the UDIV32 routine from the book:
; Assembly Language and Systems Programming for the M68000 Family, Second Edition
; by William Ford & William Topp
; Jones and Bartlett Publishers
; Pages 341, 342
ui32_divmod:
    inline
        movem.l d2 - d4, -(sp)
        move.l  d1, d2          ; is the divisor a 16bit number?
        swap    d2              ; d2 = $0000ffff
        tst.w   d2
        bne.s   .BigDivisor     ; no -> use the full division algorithm
        move.l  d0, d2          ; is high(dividend) >= divisor ?
        swap    d2
        cmp.w   d2, d1
        bhs.s   .BigDivisor     ; yes -> use the full division algorithm

        ; we can use the 32bit / 16bit = 16bit DIVU instruction
        move.l  d0, d2
        divu    d1, d2
        swap    d2              ; extract remainder to d1; isolate quotient in d0
        moveq   #0, d1
        move.w  d2, d1
        swap    d2
        moveq   #0, d0
        move.w  d2, d0
        movem.l (sp)+, d2 - d4
        rts

.BigDivisor:
        exg     d0, d1          ; d1: dividend, d0: divisor
        clr.l   d3              ; clear d3 the quotient
udiv1:  cmp.l   d0, d1          ; divisor > dividend ?
        blo.s   udiv5           ; if yes, done
        move.l  d0, d2          ; set divisor in temp register
        clr.l   d4              ; clear partial quotient
        move.w  #$10, ccr       ; set x bit to 1
udiv2:  cmp.l   d2, d1          ; divisor > dividend ?
        blo.s   udiv3           ; if yes, this step is done
        roxl.l  #1, d4          ; shift temp quotient left
        tst.l   d2              ; test for another shift
        bmi.s   udiv4           ; if bit32 == 1, don't undo shift
        lsl.l   #1, d2          ; shift divisor for next test
        bra.s   udiv2           ; keep going
udiv3:  lsr.l   #1, d2          ; undo the last shift
udiv4:  sub.l   d2, d1          ; subtract shifted divisor
        add.l   d4, d3          ; add tea quotient to d3
        bra.s   udiv1           ; go back for another division
udiv5:  move.l  d3, d0          ; put the quotient in d0
        movem.l (sp)+, d2 - d4
        rts
    einline

    endif
