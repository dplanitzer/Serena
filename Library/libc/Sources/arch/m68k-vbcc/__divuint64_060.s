;
;  arch/m68k-vbcc/__divuint64_060.s
;  libsc
;
;  Created by Dietmar Planitzer on 12/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef __divuint64_060
    xref __divmodu64


;-------------------------------------------------------------------------------
; unsigned long long _divuint64_060(unsigned long long dividend, unsigned long long divisor)
__divuint64_060:
            cargs   #(8 + 4), dividend_h.l, dividend_l.l, divisor_h.l, divisor_l.l
quotient_l  equ     4
quotient_h  equ     0

    subq.l  #8, sp  ; quotient

    clr.l   -(sp)
    pea     4 + quotient_h(a7)
    pea     8 + dividend_h(a7)
    jsr     __divmodu64

    move.l  12 + quotient_h(a7), d0
    move.l  12 + quotient_l(a7), d1
    
    add.l   #(12 + 8), sp
    rts
