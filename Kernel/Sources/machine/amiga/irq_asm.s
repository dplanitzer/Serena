;
;  machine/amiga/irq_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 8/12/25.
;  Copyright © 2025 Dietmar Planitzer. All rights reserved.
;

    include <machine/amiga/chipset.i>
    include <machine/lowmem.i>

    xdef _irq_enable
    xdef _irq_disable
    xdef _irq_restore



;-------------------------------------------------------------------------------
; void irq_enable(void)
; Enables interrupt handling.
_irq_enable:
    and.w   #$f8ff, sr
    rts

;-------------------------------------------------------------------------------
; int irq_disable(void)
; Disables interrupt handling and returns the previous interrupt handling state.
; Returns the old IRQ state
_irq_disable:
    DISABLE_INTERRUPTS d0
    rts


;-------------------------------------------------------------------------------
; void irq_restore(int state)
; Restores the given interrupt handling state.
_irq_restore:
    cargs cri_state.l
    move.l  cri_state(sp), d0
    RESTORE_INTERRUPTS d0
    rts
