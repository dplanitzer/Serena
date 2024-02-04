;
;  bitops64_m68k.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    xdef __rshsint64
    xdef __rshuint64

    xdef __lshint64
    xdef __lshuint64


;-------------------------------------------------------------------------------
; Int64 __rshsint64(Int64 x, Int32 s)
; 64bit arithmetic shift right
__rshsint64:
    inline
    cargs rsi64_saved_d7.l, rsi64_xh.l, rsi64_xl.l, rsi64_s.l
        movem.l d7, -(sp)
        move.l  rsi64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  rsi64_xh(sp), d0
        move.l  rsi64_xl(sp), d1
.L2:
        asr.l   #1, d0                  ; shift right by one bit per loop iteration
        roxr.l  d1
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
        rts
    einline

;-------------------------------------------------------------------------------
; UInt64 __rshuint64(UInt64 x, Int32 s)
; 64bit logical shift right
__rshuint64:
    inline
    cargs rui64_saved_d7.l, rui64_xh.l, rui64_xl.l, rui64_s.l
        movem.l d7, -(sp)
        move.l  rui64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  rui64_xh(sp), d0
        move.l  rui64_xl(sp), d1
.L2:
        lsr.l   #1, d0                  ; shift right by one bit per loop iteration
        roxr.l  d1
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
        rts
    einline


;-------------------------------------------------------------------------------
; Int64 __lshint64(Int64 x, Int32 s)
; UInt64 __lshuint64(UInt64 x, Int32 s)
; 64bit logical shift left
__lshint64:
__lshuint64:
    inline
    cargs lsi64_saved_d7.l, lsi64_xh.l, lsi64_xl.l, lsi64_s.l
        movem.l d7, -(sp)
        move.l  lsi64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  lsi64_xh(sp), d0
        move.l  lsi64_xl(sp), d1
.L2:
        lsl.l   #1, d1                  ; shift left by one bit per loop iteration
        roxl.l  d0
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
        rts
    einline
