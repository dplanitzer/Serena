;
;  __rshuint64.s
;  libsc
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    xdef __rshuint64


;-------------------------------------------------------------------------------
; uint64_t __rshuint64(uint64_t x, int32_t s)
; 64bit logical shift right
__rshuint64:
    inline
    cargs rui64_saved_d5.l, rui64_saved_d6.l, rui64_saved_d7.l, rui64_xh.l, rui64_xl.l, rui64_s.l
        movem.l d5-d7, -(sp)
        move.l  rui64_s(sp), d7
        move.l  rui64_xl(sp), d1
        and.l   #$3f, d7
        bne.s   .L1
        ; Shift by 0 -> nothing to do
        move.l  rui64_xh(sp), d0
        bra.s   .done
.L1:
        cmp.l   #32, d7
        blt.s   .L2
        ; Shift by >= 32 bits -> high word == 0; shift low word
        moveq.l #0, d0
        sub.b   #32, d7
        lsr.l   d7, d1
        bra.s   .done
.L2:
        ; Shift by < 32 bits -> extract N top bits from high word; insert in low word; shift high and low word
        move.l  rui64_xh(sp), d0
        move.l  #32, d5
        sub.l   d7, d5
        bfextu  d0{d5:d7}, d6
        lsr.l   d7, d0
        lsr.l   d7, d1
        bfins   d6, d1{0:d7}
.done:
        movem.l (sp)+, d5-d7
        rts
    einline
