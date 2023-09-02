;
;  UInt64_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


;-------------------------------------------------------------------------------
; 64bit unsigned arithmetic
;-------------------------------------------------------------------------------


    xdef __rshuint64
    xdef __lshuint64


;-------------------------------------------------------------------------------
; UInt64 __rshuint64(UInt64 x, Int32 s)
; 64bit unsigned shift right
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
; UInt64 __lshuint64(UInt64 x, Int32 s)
; 64bit unsigned shift left
__lshuint64:
    inline
    cargs lui64_saved_d7.l, lui64_xl.l, lui64_xh.l, lui64_s.l
        move.l  lui64_s(sp), d7
        and.l   #$3f, d7                ; the shift range is 0 - 63
        beq.s   .L1
        subq.l  #1, d7                  ; prepare the dbra loop
        move.l  lui64_xh(sp), d0
        move.l  lui64_xl(sp), d1
.L2:
        lsl.l   #1, d0                  ; shift left by one bit per loop iteration
        roxl.l  d1
        dbra    d7, .L2
.L1:
        movem.l (sp)+, d7
        rts
    einline
