;
;  machine/hw/m68k/sigurgent.s
;  kernel
;
;  Created by Dietmar Planitzer on 9/2/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    xdef _sigurgent
    xdef _sigurgent_end


;-------------------------------------------------------------------------------
; void sigurgent(void)
; void sigurgent_end(void)
_sigurgent:
    move.l  #63, -(sp)   ; SC_sigurgent
    subq.l  #4, sp
    trap    #0
    addq.l  #8, sp
    rts
_sigurgent_end:
    nop
