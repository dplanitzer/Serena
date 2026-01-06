;
;  rc.s
;  libsc
;
;  Created by Dietmar Planitzer on 7/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;


    xdef _rc_retain
    xdef _rc_getcount


;-------------------------------------------------------------------------------
; void rc_retain(volatile ref_count_t* _Nonnull rc)
; Atomically increments the retain count 'rc'.
; IRQ safe
_rc_retain:
    inline
    cargs retain_rc_ptr.l
        move.l  retain_rc_ptr(sp), a0
        addq.l  #1, (a0)
        rts
    einline

;-------------------------------------------------------------------------------
; int rc_getcount(volatile ref_count_t* _Nonnull rc)
; Returns a copy of the current retain count. This should be used for debugging
; purposes only.
; IRQ safe
_rc_getcount:
    inline
    cargs getcount_rc_ptr.l
        move.l  getcount_rc_ptr(sp), d0
        rts
    einline
