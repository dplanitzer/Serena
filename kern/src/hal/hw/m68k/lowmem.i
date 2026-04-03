;
;  machine/hw/m68k/lowmem.i
;  kernel
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

        ifnd LOWMEM_I
LOWMEM_I    set 1


; RGB color
RGB_RED             equ $0f00
RGB_GREEN           equ $00f0
RGB_BLUE            equ $000f
RGB_YELLOW          equ $0ff0
RGB_WHITE           equ $0fff
RGB_BLACK           equ $0000


;
; The boot services memory region of the kernel. Boot services memory starts at
; physical address 0 and extends to physical address 12288. The first page holds
; the CPU vectors (starting at physical address 0) and the system description
; data structure. The system description contains basic information about the
; system like CPU type and the motherboard memory layout. Pages 2 and 3 contain
; the boot environment stack.
;
; This is what the memory map looks like at reset time:
;
; ------------------------------------------------------------------------------
; | CPU vectors | system description | empty | boot stack (8K) | chip RAM ...
; ------------------------------------------------------------------------------
;
; The _Reset function initializes the CPU vector table and it clears the system
; description  memory region. It then initializes the system description with a
; single empty memory descriptor. Note however that the memory descriptor lower
; and upper bound is set to the end of the boot services memory region. That way
; the first memory descriptor will exclude the boot services memory region once
; the motherboard memory check has completed.  
; _Reset then calls OnReset() which takes care setting up the kernel data and
; bss segments.
; Once OnReset() returns, _Reset calls OnBoot(). OnBoot() figures out where the
; kernel stack should live in memory and how big it should be. It tries to put
; the kernel stack into fast RAM.
; Once OnBoot() has returned, _Reset triggers the first context switch to the
; boot virtual processor that was set up in OnBoot(). The boot virtual processor
; then executes OnStartup() which finishes the initialization of the kernel.
;
CPU_VECTORS_BASE            equ     0
CPU_VECTORS_SIZEOF          equ     1024

SYS_DESC_BASE               equ     CPU_VECTORS_SIZEOF
SYS_DESC_SIZEOF             equ     1024

RESET_STACK_BASE            equ     3*4096
RESET_STACK_SIZEOF          equ     8192

BOOT_SERVICES_MEM_TOP       equ     RESET_STACK_BASE



; The sys_desc object
    clrso
sd_cpu_type                     so.l    1       ; 4
sd_cpu_subtype                  so.l    1       ; 4
sd_cipset_version               so.b    1       ; 1
sd_chipset_ramsey_version       so.b    1       ; 1
sd_reserved1                    so.b    1       ; 1
sd_reserved2                    so.b    1       ; 1
sd_chipset_upper_dma_limit      so.l    1       ; 4
sd_memory_desc_count            so.l    1       ; 4
sd_memory_desc                  so.b    12*8    ; 12 * 8
sd_SIZEOF                       so
    ifeq (sd_SIZEOF == 116)
        fail "sys_desc structure size is incorrect."
    endif


; The struct clock object
    clrso
mtc_tick_count                  so.l    1       ; 4
mtc_deadline_queue              so.l    1       ; 4
mtc_ns_per_tick                 so.l    1       ; 4
mtc_cia_cycles_per_tick         so.w    1       ; 2
mtc_ns_per_cia_cycle            so.w    1       ; 2
mtc_SIZEOF                      so
    ifeq (mtc_SIZEOF == 16)
        fail "clock_ref_t structure size is incorrect."
    endif


; The vcpu scheduler
CSWB_SIGNAL_SWITCH                  equ     0
CSWB_SIGNAL_PREEMPTED               equ     1

SCHED_PRI_COUNT                     equ     80

    clrso
sched_running                       so.l    1       ; 4
sched_scheduled                     so.l    1       ; 4
sched_csw_signals                   so.b    1       ; 1
sched_flags                         so.b    1       ; 1
sched_irq_nest_count                so.b    1       ; 1
sched_reserved2                     so.b    1       ; 1
sched_idle_virtual_processor        so.l    1       ; 4
sched_boot_virtual_processor        so.l    1       ; 4
sched_finalizer_queue_first         so.l    1       ; 4
sched_finalizer_queue_last          so.l    1       ; 4
sched_ready_queue                   so.l    SCHED_PRI_COUNT * 2 ; 640
sched_ready_queue_populated         so.l    3       ; 3
sched_SIZEOF                        so
    ifeq (sched_SIZEOF == 680)
        fail "sched_t structure size is incorrect."
    endif


; The vcpu
VCPU_RUST_INITIATED       equ 0   ; VP was just created and has not been scheduled yet
VCPU_RUST_READY           equ 1   ; VP is able to run and is currently sitting on the ready queue
VCPU_RUST_RUNNING         equ 2   ; VP is running
VCPU_RUST_WAITING         equ 3   ; VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc)
VCPU_RUST_SUSPENDED       equ 4   ; VP was running or ready and is now suspended (it is not on any queue)
VCPU_RUST_TERMINATING     equ 5   ; VP is in the process of terminating and being reaped (it's on the finalizer queue)


; vcpu flags (keep in sync with vcpu.h)
VP_FLAG_HAS_FPU_BIT     equ 0   ; Save/restore the FPU state
VP_FLAG_HAS_BC_BIT      equ 1   ; Clear branch cache on context switch

    clrso
vp_rewa_qe_next                         so.l    1           ; 4
vp_rewa_qe_prev                         so.l    1           ; 4
vp_csw_sa                               so.l    1           ; 4
vp_syscall_sa                           so.l    1           ; 4
vp_kernel_stack_base                    so.l    1           ; 4
vp_kernel_stack_size                    so.l    1           ; 4
vp_user_stack_base                      so.l    1           ; 4
vp_user_stack_size                      so.l    1           ; 4
vp_id                                   so.l    1           ; 4
vp_groupid                              so.l    1           ; 4
vp_owner_qe_next                        so.l    1           ; 4
vp_owner_qe_prev                        so.l    1           ; 4
vp_uerrno                               so.l    1           ; 4
vp_udata                                so.l    1           ; 4
vp_excpt_handler_func                   so.l    1           ; 4
vp_excpt_handler_arg                    so.l    1           ; 4
vp_excpt_handler_flags                  so.l    1           ; 4
vp_excpt_sa                             so.l    1           ; 4
vp_excpt_state_sp                       so.l    1           ; 4
vp_excpt_state_pc                       so.l    1           ; 4
vp_excpt_state_addr                     so.l    1           ; 4
vp_excpt_state_code                     so.w    1           ; 2
vp_excpt_state_cpu_code                 so.w    1           ; 2
vp_pending_sigs                         so.l    1           ; 4
vp_timeout_next                         so.l    1           ; 4
vp_timeout_deadline                     so.l    1           ; 4
vp_timeout_func                         so.l    1           ; 4
vp_timeout_arg                          so.l    1           ; 4
vp_timeout_is_armed                     so.b    1           ; 1
vp_timeout_reserved                     so.b    3           ; 3
vp_waiting_on_wait_queue                so.l    1           ; 4
vp_wait_sigs                            so.l    1           ; 4
vp_wakeup_reason                        so.b    1           ; 1
vp_base_priority                        so.b    1           ; 1
vp_cur_priority                         so.b    1           ; 1
vp_priority_penalty                     so.b    1           ; 1
vp_priority_boost                       so.b    1           ; 1
vp_quantum_boost                        so.b    1           ; 1
vp_sched_nice                           so.b    1           ; 1
vp_run_state                            so.b    1           ; 1
vp_flags                                so.b    1           ; 1
vp_quantum_countdown                    so.b    1           ; 1
vp_suspension_count                     so.w    1           ; 2
vp_acquisition_time_secs                so.l    1           ; 4
vp_acquisition_time_nanos               so.l    1           ; 4
vp_user_ticks                           so.l    1           ; 4
vp_system_ticks                         so.l    1           ; 4
vp_wait_ticks                           so.l    1           ; 4
vp_proc                                 so.l    1           ; 4
vp_dispatch_worker                      so.l    1           ; 4
vp_SIZEOF                               so
    ifeq (vp_SIZEOF == 160)
        fail "vcpu structure size is incorrect."
    endif

        endif ; LOWMEM_I
