;
;  MonotonicClock_asm.i
;  kernel
;
;  Created by Dietmar Planitzer on 2/11/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include <machine/amiga/chipset.i>
    include <machine/lowmem.i>

    xref _gMonotonicClockStorage

    xdef _MonotonicClock_GetCurrentQuantums


;-------------------------------------------------------------------------------
; Quantums MonotonicClock_GetCurrentQuantums(void)
; Returns the current time in terms of quantums
_MonotonicClock_GetCurrentQuantums:
    move.l  _gMonotonicClockStorage + mtc_current_quantum, d0
    rts
