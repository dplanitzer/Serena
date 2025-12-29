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
; based on: https://stackoverflow.com/questions/12041632/how-to-compute-the-integer-absolute-value
_abs:
_labs:
    cargs abs_n.l
    move.l  abs_n(sp), d0   ; d0 := n
    move.l  d2, a0
    move.l  d0, d2
    moveq.l #31, d1
    asr.l   d1, d2          ; mask = n >> 31 | d2 := mask
    eor.l   d2, d0          ; d0 := mask ^ n
    sub.l   d2, d0          ; r = (mask ^ n) - mask
    move.l  a0, d2
    rts
