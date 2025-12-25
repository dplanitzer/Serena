;
;  bitops64_m68k.s
;  libsc
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    xdef __rshsint64
    xdef __rshuint64

    xdef __lshint64
    xdef __lshuint64


;-------------------------------------------------------------------------------
; int64_t __rshsint64(int64_t x, int32_t s)
; 64bit arithmetic shift right
__rshsint64:
    inline
    cargs rsi64_saved_d5.l, rsi64_saved_d6.l, rsi64_saved_d7.l, rsi64_xh.l, rsi64_xl.l, rsi64_s.l
        movem.l d5-d7, -(sp)
        move.l  rsi64_s(sp), d7
        move.l  rsi64_xl(sp), d1
        move.l  rsi64_xh(sp), d0
        and.l   #$3f, d7
        bne.s   .L1
        ; Shift by 0 -> nothing to do
        bra.s   .done
.L1:
        cmp.l   #32, d7
        blt.s   .L2
        ; Shift by >= 32 bits -> high word == 0; shift low word
        tst.l   d0
        bmi.s   .L11
        moveq.l #0, d0
        and     #$0f, ccr
        bra.s   .L12
.L11:
        moveq.l #0, d0
        not.l   d0
        or      #$10, ccr
.L12:
        move    ccr, d6
        sub.b   #32, d7
        beq.s   .done
        move    d6, ccr
        roxr.l  #1, d1      ; shifts the X bit (which holds our high word MSB) into the low word MSB and drops the low word LSB
        subq.l  #1, d7
        asr.l   d7, d1      ; remaining bits are shifted with a regular arithmetic shift right because our low word sign bit is now correct
        bra.s   .done
.L2:
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
