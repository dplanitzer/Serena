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
; based on: https://stackoverflow.com/questions/12041632/how-to-compute-the-integer-absolute-value
_llabs:
_imaxabs:
    cargs abs_nh.l, abs_nl.l
    move.l  abs_nh(sp), d0   ; (d0, d1) := (n_h, n_l)
    move.l  abs_nl(sp), d1
    
    move.l  d2, a0
    move.l  d3, a1

    moveq.l #31, d3
    asr.l   d3, d2          ; mask = n_h >> 31 | d2 := mask; mask_l == mask_h

    eor.l   d2, d0          ; d0 := mask ^ n_h
    eor.l   d2, d1          ; d1 := mask ^ n_l

    sub.l   d2, d0          ; r_h = (mask ^ n_h) - mask
    sub.l   d2, d1          ; r_l = (mask ^ n_l) - mask

    move.l  a0, d2
    move.l  a1, d3
    rts
