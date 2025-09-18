;
;  preempt.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/23/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include <machine/lowmem.i>

    xdef _preempt_disable
    xdef _preempt_restore


;-------------------------------------------------------------------------------
; int preempt_disable(void)
; Disables preemption and returns the previous preemption state.
_preempt_disable:
    DISABLE_PREEMPTION d0
    rts


;-------------------------------------------------------------------------------
; void preempt_restore(int sps)
; Restores the preemption state to 'sps'. Note that this function call wipes out
; the condition codes and tracing enabled state.
_preempt_restore:
    cargs rp_state.l
    move.l  rp_state(sp), d0
    RESTORE_PREEMPTION d0
    rts
