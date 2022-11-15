;
;  VirtualProcessor_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 7/20/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "syscalls.i"

    xdef _VirtualProcessor_ReturnFromUserSpace


;-------------------------------------------------------------------------------
; void VirtualProcessor_ReturnFromUserSpace(void)
_VirtualProcessor_ReturnFromUserSpace:
    SYSCALL SC_EXIT_VP
    ; NOT REACHED
