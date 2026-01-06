;
;  rc_kei.s
;  libc
;
;  Created by Dietmar Planitzer on 7/30/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    include <machine/amiga/kei.i>


    xdef _rc_release


;-------------------------------------------------------------------------------
; int rc_release(volatile ref_count_t* _Nonnull rc)
; Atomically releases a single strong reference and adjusts the retain count
; accordingly. Returns true if the retain count has reached zero and the caller
; should destroy the associated resources.
_rc_release:
    inline
    cargs release_rc_ptr.l
        move.l  release_rc_ptr(sp), a0
        dc.w    RC_RELEASE
        rts
    einline
