;
;  __rshsint64.s
;  libsc
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    xdef __rshsint64


;-------------------------------------------------------------------------------
; int64_t __rshsint64(int64_t x, int32_t s)
; 64bit arithmetic shift right
__rshsint64:
    inline
    cargs #(12 + 4), rsi64_xh.l, rsi64_xl.l, rsi64_s.l
        movem.l d5-d7, -(sp)

        move.l  rsi64_xh(sp), d0
        move.l  rsi64_xl(sp), d1

        move.l  rsi64_s(sp), d7
        and.l   #$3f, d7
        bne.s   .do_shift
        ; Shift by 0 -> nothing to do
        bra.s   .done

.do_shift:
        cmp.l   #32, d7
        blt.s   .shift_by_less_than_32
        ; Shift by >= 32 bits -> high word == 0; shift low word
        sub.b   #32, d7
        move.l  d0, d1
        asr.l   d7, d1
        tst.l   d0
        bmi.s   .shift_neg_by_at_least_32
        moveq.l #0, d0
        bra.s   .done

.shift_neg_by_at_least_32:
        moveq.l #0, d0
        not.l   d0
        bra.s   .done

.shift_by_less_than_32:
        ; Shift by < 32 bits -> extract N top bits from high word; insert in low word; shift high and low word
        move.l  #32, d5
        sub.l   d7, d5
        bfextu  d0{d5:d7}, d6

        asr.l   d7, d0
        lsr.l   d7, d1
        
        bfins   d6, d1{0:d7}

.done:
        movem.l (sp)+, d5-d7
        rts
    einline
