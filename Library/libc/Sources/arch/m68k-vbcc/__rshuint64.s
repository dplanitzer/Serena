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
    cargs #(12 + 4), rui64_xh.l, rui64_xl.l, rui64_s.l
        movem.l d5-d7, -(sp)

        move.l  rui64_xh(sp), d0
        move.l  rui64_xl(sp), d1

        move.l  rui64_s(sp), d7
        and.l   #$3f, d7
        bne.s   .do_shift
        ; Shift by 0 -> nothing to do
        bra.s   .done

.do_shift:
        cmp.b   #32, d7
        blt.s   .shift_by_less_than_32
        ; Shift by >= 32 bits -> high word == 0; shift low word
        sub.b   #32, d7
        move.l  d0, d1
        lsr.l   d7, d1
        moveq.l #0, d0
        bra.s   .done

.shift_by_less_than_32:
        ; Shift by < 32 bits -> extract N top bits from high word; insert in low word; shift high and low word
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
