;
;  __lshint64.s
;  libsc
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    xdef __lshint64
    xdef __lshuint64


;-------------------------------------------------------------------------------
; int64_t __lshint64(int64_t x, int32_t s)
; uint64_t __lshuint64(uint64_t x, int32_t s)
; 64bit logical shift left
__lshint64:
__lshuint64:
    inline
    cargs lsi64_saved_d5.l, lsi64_saved_d6.l, lsi64_saved_d7.l, lsi64_xh.l, lsi64_xl.l, lsi64_s.l
        movem.l d5-d7, -(sp)
        move.l  lsi64_s(sp), d7
        move.l  lsi64_xh(sp), d0
        and.l   #$3f, d7
        bne.s   .L1
        ; Shift by 0 -> nothing to do
        move.l  lsi64_xl(sp), d1
        bra.s   .done
.L1:
        cmp.l   #32, d7
        blt.s   .L2
        ; Shift by >= 32 bits -> low word == 0; shift high word
        moveq.l #0, d1
        sub.b   #32, d7
        lsl.l   d7, d0
        bra.s   .done
.L2:
        ; Shift by < 32 bits -> extract N top bits from low word; insert in high word; shift low and high word
        move.l  lsi64_xl(sp), d1
        bfextu  d1{0:d7}, d6
        lsl.l   d7, d1
        lsl.l   d7, d0
        move.l  #32, d5
        sub.l   d7, d5
        bfins   d6, d0{d5:d7}
.done:
        movem.l (sp)+, d5-d7
        rts
    einline
