;
;  arch/m68k/bit.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 1/7/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _leading_zeros_uc
    xdef _leading_zeros_us
    xdef _leading_zeros_ui
    xdef _leading_zeros_ul
    xdef _leading_zeros_ull

    xdef _leading_ones_uc
    xdef _leading_ones_us
    xdef _leading_ones_ui
    xdef _leading_ones_ul
    xdef _leading_ones_ull



;-------------------------------------------------------------------------------
; unsigned int leading_zeros_uc(unsigned char val);
; unsigned int leading_zeros_us(unsigned short val);
; unsigned int leading_zeros_ui((unsigned long)__n)
; unsigned int leading_zeros_ul(unsigned long val);
; unsigned int leading_zeros_ull(unsigned long long val);
_leading_zeros_uc:
    cargs uc_val.l
    bfffo   uc_val(sp){24:8}, d0
    sub.b   #24, d0
    rts

_leading_zeros_us:
    cargs us_val.l
    bfffo   us_val(sp){16:16}, d0
    sub.b   #16, d0
    rts

_leading_zeros_ui:
_leading_zeros_ul:
    cargs ul_val.l
    bfffo   ul_val(sp){0:32}, d0
    rts

_leading_zeros_ull:
    cargs ull_val_h.l, ull_val_l.l
    bfffo   ull_val_h(sp){0:32}, d0
    bne.s   .done
    bfffo   ull_val_l(sp){0:32}, d0
    add.b   #32, d0
.done:
    rts



;-------------------------------------------------------------------------------
; unsigned int leading_ones_uc(unsigned char val);
; unsigned int leading_ones_us(unsigned short val);
; unsigned int leading_ones_ui((unsigned long)__n)
; unsigned int leading_ones_ul(unsigned long val);
; unsigned int leading_ones_ull(unsigned long long val);
_leading_ones_uc:
    cargs ouc_val.l
    move.l  ouc_val(sp), d1
    neg.l   d1
    bfffo   d1{24:8}, d0
    sub.b   #24, d0
    rts

_leading_ones_us:
    cargs ous_val.l
    move.l  ous_val(sp), d1
    neg.l   d1
    bfffo   d1{0:16}, d0
    sub.b   #16, d0
    rts

_leading_ones_ui:
_leading_ones_ul:
    cargs oul_val.l
    move.l  oul_val(sp), d1
    neg.l   d1
    bfffo   d1{0:32}, d0
    rts

_leading_ones_ull:
    cargs oull_val_h.l, oull_val_l.l
    move.l  oull_val_h(sp), d1
    neg.l   d1
    bfffo   d1{0:32}, d0
    bne.s   .done
    move.l  oull_val_l(sp), d1
    neg.l   d1
    bfffo   d1{0:32}, d0
    add.b   #32, d0
.done:
    rts
