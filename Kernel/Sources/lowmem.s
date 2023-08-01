;
;  lowmem.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "lowmem.i"

    xref _gVirtualProcessorSchedulerStorage

    xdef _VirtualProcessor_GetCurrent
    xdef _VirtualProcessor_GetCurrentVpid


;-------------------------------------------------------------------------------
; VirtualProcessor* VirtualProcessor_GetCurrent(void)
; Returns a reference to the currently running virtual processor.
_VirtualProcessor_GetCurrent:
    move.l  _gVirtualProcessorSchedulerStorage + vps_running, d0
    rts


;-------------------------------------------------------------------------------
; Int VirtualProcessor_GetCurrentVpid(void)
; Returns the VPID of the currently running virtual processor.
_VirtualProcessor_GetCurrentVpid:
    move.l  _gVirtualProcessorSchedulerStorage + vps_running, a0
    move.l  vp_vpid(a0), d0
    rts
