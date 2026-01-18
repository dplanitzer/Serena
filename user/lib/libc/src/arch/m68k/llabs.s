;
;  arch/m68k/llabs.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 12/28/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef _llabs
    xdef _imaxabs



;-------------------------------------------------------------------------------
; long long llabs(long long n)
; intmax_t imaxabs(intmax_t n)
_llabs:
_imaxabs:
    cargs abs_nh.l, abs_nl.l
    move.l  abs_nl(sp), d1
    move.l  abs_nh(sp), d0   ; (d0, d1) := (n_h, n_l)
    bpl.l   .1

    neg.l   d1
    negx.l  d0
.1:
    rts
