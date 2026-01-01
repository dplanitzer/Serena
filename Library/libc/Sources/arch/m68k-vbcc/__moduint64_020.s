;
;  arch/m68k-vbcc/__moduint64_020.s
;  libsc
;
;  Created by Dietmar Planitzer on 12/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef __moduint64_020
    xref __divu64


;-------------------------------------------------------------------------------
; unsigned long long _moduint64_020(unsigned long long dividend, unsigned long long divisor)
__moduint64_020:
            cargs   #(16 + 4), dividend_h.l, dividend_l.l, divisor_h.l, divisor_l.l
quotient_l  equ     12
quotient_h  equ     8
remainder_l equ     4
remainder_h equ     0

    sub.l   #16, sp  ; remainder, quotient

    pea     0 + remainder_h(a7)
    pea     4 + quotient_h(a7)
    pea     8 + dividend_h(a7)
    jsr     __divu64

    move.l  12 + remainder_h(a7), d0
    move.l  12 + remainder_l(a7), d1
    
    add.l   #(12 + 16), sp
    rts
