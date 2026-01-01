;
;  arch/m68k-vbcc/__modsint64_020.s
;  libsc
;
;  Created by Dietmar Planitzer on 12/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef __modsint64_020
    xref __divmods64


;-------------------------------------------------------------------------------
; long long _modsint64_020(long long dividend, long long divisor)
__modsint64_020:
            cargs   #(16 + 4), dividend_h.l, dividend_l.l, divisor_h.l, divisor_l.l
quotient_l  equ     12
quotient_h  equ     8
remainder_l equ     4
remainder_h equ     0

    move.l  -16 + dividend_h(sp), d0
    move.l  -16 + divisor_h(sp), d1
    bne.s   .do_full_div
    tst.l   d0
    bne.s   .do_full_div

    move.l  -16 + dividend_l(sp), d0
    move.l  -16 + divisor_l(sp), d1
    divsl.l d1, d1:d0       ; d0/d1 = remainder_l:quotient_l
    moveq.l #0, d0
    rts

.do_full_div:
    sub.l   #16, sp  ; remainder, quotient

    pea     0 + remainder_h(a7)
    pea     4 + quotient_h(a7)
    pea     8 + dividend_h(a7)
    jsr     __divmods64

    move.l  12 + remainder_h(a7), d0
    move.l  12 + remainder_l(a7), d1

    add.l   #(12 + 16), sp
    rts
