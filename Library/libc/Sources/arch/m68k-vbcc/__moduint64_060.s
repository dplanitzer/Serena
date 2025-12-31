;
;  arch/m68k-vbcc/__moduint64_060.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 12/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef __moduint64_060
    xref __divu64


;-------------------------------------------------------------------------------
; unsigned long long _moduint64_060(unsigned long long dividend, unsigned long long divisor)
__moduint64_060:
            cargs   dividend_h.l, dividend_l.l, divisor_h.l, divisor_l.l
quotient_l  equ     -4
quotient_h  equ     -8
remainder_l equ     -12
remainder_h equ     -16

    move.l  sp, a0  ; a0 := base ptr

    sub.l   #16, sp  ; remainder, quotient

    pea     remainder_h(a0)
    pea     quotient_h(a0)
    pea     dividend_h(a0)
    jsr     __divu64
    add.l   #12, sp

    move.l  0(a7), d0
    move.l  4(a7), d1
    add.l   #16, sp
    rts
