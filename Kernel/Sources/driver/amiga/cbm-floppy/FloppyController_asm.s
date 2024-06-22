;
;  FloppyController_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/12/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "../hal/chipset.i"
    include "../hal/lowmem.i"

    xdef _fdc_nano_delay


;-------------------------------------------------------------------------------
; void fdc_nano_delay(void)
; Waits a few nanoseconds.
_fdc_nano_delay:
    nop
    nop
    rts
