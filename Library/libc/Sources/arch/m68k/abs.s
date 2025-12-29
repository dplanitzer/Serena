;
;  arch/m68k/abs.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 12/28/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    xdef _abs
    xdef _labs



;-------------------------------------------------------------------------------
; int abs(int n)
; long labs(long n)
_abs:
_labs:
    cargs abs_n.l
    move.l  abs_n(sp), d0   ; d0 := n
    bpl.s   .1
    neg.l   d0
.1:
    rts
