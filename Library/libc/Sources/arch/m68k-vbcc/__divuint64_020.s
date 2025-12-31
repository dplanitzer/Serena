;
;  arch/m68k-vbcc/__divuint64_020.s
;  libsc
;
;  Created by Dietmar Planitzer on 12/29/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef __divuint64_020
    xref __divu64


;-------------------------------------------------------------------------------
; unsigned long long _divuint64_020(unsigned long long dividend, unsigned long long divisor)
__divuint64_020:
            cargs   dividend_h.l, dividend_l.l, divisor_h.l, divisor_l.l
quotient_l  equ     -4
quotient_h  equ     -8

    move.l  sp, a0  ; a0 := base ptr

    subq.l  #8, sp  ; quotient

    clr.l   -(sp)
    pea     quotient_h(a0)
    pea     dividend_h(a0)
    jsr     __divu64
    add.l   #12, sp

    move.l  0(a7), d0
    move.l  4(a7), d1
    addq.l  #8, sp
    rts
