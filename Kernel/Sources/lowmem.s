;
;  lowmem.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "lowmem.i"

    xdef _SystemDescription_GetShared
    xdef _MonotonicClock_GetShared
    xdef _VirtualProcessorScheduler_GetShared
    xdef _BootVirtualProcessor_GetShared
    xdef _VirtualProcessor_GetCurrent
    xdef _VirtualProcessor_GetCurrentVpid
    xdef _InterruptController_GetShared
    xdef _SystemGlobals_Get


;-------------------------------------------------------------------------------
; SystemDescription* SystemDescription_GetShared(void)
; Returns a reference to the shared system description struct.
_SystemDescription_GetShared:
    move.l  #SYS_DESC_BASE, d0
    rts

;-------------------------------------------------------------------------------
; MonotonicClock* MonotonicClock_GetShared(void)
; Returns a reference to the monotonic clock.
_MonotonicClock_GetShared:
    move.l  #MONOTONIC_CLOCK_BASE, d0
    rts

;-------------------------------------------------------------------------------
; VirtualProcessorScheduler* VirtualProcessorScheduler_GetShared(void)
; Returns a reference to the virtual processor scheduler.
_VirtualProcessorScheduler_GetShared:
    move.l  #SCHEDULER_BASE, d0
    rts

;-------------------------------------------------------------------------------
; VirtualProcessor* BootVirtualProcessor_GetShared(void)
; Returns a reference to the boot virtual processor.
_BootVirtualProcessor_GetShared:
    move.l  #SCHEDULER_VP_BASE, d0
    rts


;-------------------------------------------------------------------------------
; VirtualProcessor* VirtualProcessor_GetCurrent(void)
; Returns a reference to the currently running virtual processor.
_VirtualProcessor_GetCurrent:
    move.l  SCHEDULER_BASE + vps_running, d0
    rts


;-------------------------------------------------------------------------------
; Int VirtualProcessor_GetCurrentVpid(void)
; Returns the VPID of the currently running virtual processor.
_VirtualProcessor_GetCurrentVpid:
    move.l  SCHEDULER_BASE + vps_running, a0
    move.l  vp_vpid(a0), d0
    rts


;-------------------------------------------------------------------------------
; InterruptController* InterruptController_GetShared(void)
; Returns a reference to the interrupt controller.
_InterruptController_GetShared:
    move.l  #INTERRUPT_CONTROLLER_BASE, d0
    rts


;-------------------------------------------------------------------------------
; SystemGlobals* SystemGlobals_Get(void)
; Returns a reference to the system globals.
_SystemGlobals_Get:
    move.l  #SYSTEM_GLOBALS_BASE, d0
    rts
