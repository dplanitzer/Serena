;
;  lowmem.i
;  Apollo
;
;  Created by Dietmar Planitzer on 2/2/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

        ifnd LOWMEM_I
LOWMEM_I    set 1

    xref _gVirtualProcessorSchedulerStorage


; CPU models
CPU_MODEL_68000     equ 0
CPU_MODEL_68010     equ 1
CPU_MODEL_68020     equ 2
CPU_MODEL_68030     equ 3
CPU_MODEL_68040     equ 4
CPU_MODEL_68060     equ 6

; FPU models
FPU_MODEL_NONE      equ 0
FPU_MODEL_68881     equ 1
FPU_MODEL_68882     equ 2
FPU_MODEL_68040     equ 3
FPU_MODEL_68060     equ 4

; IRQ sources
IRQ_SOURCE_CIA_B_FLAG                   equ 23
IRQ_SOURCE_CIA_B_SP                     equ 22
IRQ_SOURCE_CIA_B_ALARM                  equ 21
IRQ_SOURCE_CIA_B_TIMER_B                equ 20
IRQ_SOURCE_CIA_B_TIMER_A                equ 19

IRQ_SOURCE_CIA_A_FLAG                   equ 18
IRQ_SOURCE_CIA_A_SP                     equ 17
IRQ_SOURCE_CIA_A_ALARM                  equ 16
IRQ_SOURCE_CIA_A_TIMER_B                equ 15
IRQ_SOURCE_CIA_A_TIMER_A                equ 14

IRQ_SOURCE_EXTERN                       equ 13
IRQ_SOURCE_DISK_SYNC                    equ 12
IRQ_SOURCE_SERIAL_RECEIVE_BUFFER_FULL   equ 11
IRQ_SOURCE_AUDIO3                       equ 10
IRQ_SOURCE_AUDIO2                       equ 9
IRQ_SOURCE_AUDIO1                       equ 8
IRQ_SOURCE_AUDIO0                       equ 7
IRQ_SOURCE_BLITTER                      equ 6
IRQ_SOURCE_VERTICAL_BLANK               equ 5
IRQ_SOURCE_COPPER                       equ 4
IRQ_SOURCE_PORTS                        equ 3
IRQ_SOURCE_SOFT                         equ 2
IRQ_SOURCE_DISK_BLOCK                   equ 1
IRQ_SOURCE_SERIAL_TRANSMIT_BUFFER_EMPTY equ 0

IRQ_SOURCE_COUNT                        equ 24

; Chipset timers
CHIPSET_TIMER_0     equ 0   ; CIA A timer A
CHIPSET_TIMER_1     equ 1   ; CIA A timer B
CHIPSET_TIMER_2     equ 2   ; CIA B timer A
CHIPSET_TIMER_3     equ 3   ; CIA B timer B


; Error Codes

EOK         equ 0
ENOMEM      equ 1
ENODATA     equ 2
ENOTCDF     equ 3
ENOBOOT     equ 4
ENODRIVE    equ 5
EDISKCHANGE equ 6
ETIMEDOUT   equ 7
ENODEV      equ 8
EPARAM      equ 9
ERANGE      equ 10
EINTR       equ 11
EAGAIN      equ 12
EPIPE       equ 13
EBUSY       equ 14
ENOSYS      equ 15
EINVAL      equ 16
EIO         equ 17
EPERM       equ 18
EDEADLK     equ 19
EDOM        equ 20
EILSEQ      equ 21
ENOEXEC     equ 22


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



; The SystemDescription object
    clrso
sd_cpu_model                    so.b    1       ; 1
sd_fpu_model                    so.b    1       ; 1
sd_cipset_version               so.b    1       ; 1
sd_chipset_ramsey_version       so.b    1       ; 1
sd_chipset_upper_dma_limit      so.l    1       ; 4
sd_quantum_duration_ns          so.l    1       ; 4
sd_quantum_duration_cycles      so.w    1       ; 2
sd_ns_per_quantum_timer_cycle   so.w    1       ; 2
sd_memory_descriptor_count      so.l    1       ; 4
sd_memory_descriptor            so.b    12*8    ; 12 * 8
sd_SIZEOF                       so              ; 116
    ifeq (sd_SIZEOF == 116)
        fail "SystemDescription structure size is incorrect."
    endif


; The MonotonicClock object
    clrso
mtc_current_time_seconds        so.l    1       ; 4
mtc_current_time_nanoseconds    so.l    1       ; 4
mtc_current_quantum             so.l    1       ; 4
mtc_ns_per_quantum              so.l    1       ; 4
mtc_SIZEOF                      so
    ifeq (mtc_SIZEOF == 16)
        fail "MonotonicClock structure size is incorrect."
    endif


; The VirtualProcessorScheduler
CSWB_SIGNAL_SWITCH                  equ     0
CSWB_HW_HAS_FPU                     equ     0
SCHED_FLAG_VOLUNTARY_CSW_ENABLED    equ     0

VP_PRIORITY_COUNT                   equ     64

    clrso
vps_running                         so.l    1       ; 4
vps_scheduled                       so.l    1       ; 4
vps_idle_virtual_processor          so.l    1       ; 4
vps_boot_virtual_processor          so.l    1       ; 4
vps_ready_queue                     so.l    VP_PRIORITY_COUNT * 2 ; 512
vps_ready_queue_populated           so.b    8       ; 8
vps_csw_scratch                     so.l    1       ; 4
vps_csw_signals                     so.b    1       ; 1
vps_csw_hw                          so.b    1       ; 1
vps_flags                           so.b    1       ; 1
vps_reserved                        so.b    1       ; 1
vps_quantums_per_quarter_second     so.l    1       ; 4
vps_timeout_queue_first             so.l    1       ; 4
vps_timeout_queue_last              so.l    1       ; 4
vps_sleep_queue_first               so.l    1       ; 4
vps_sleep_queue_last                so.l    1       ; 4
vps_scheduler_wait_queue_first      so.l    1       ; 4
vps_scheduler_wait_queue_last       so.l    1       ; 4
vps_finalizer_queue_first           so.l    1       ; 4
vps_finalizer_queue_last            so.l    1       ; 4
vps_SIZEOF                          so
    ifeq (vps_SIZEOF == 580)
        fail "VirtualProcessorScheduler structure size is incorrect."
    endif


; The CpuContext
    clrso
cpu_d0              so.l    1       ; 4
cpu_d1              so.l    1       ; 4
cpu_d2              so.l    1       ; 4
cpu_d3              so.l    1       ; 4
cpu_d4              so.l    1       ; 4
cpu_d5              so.l    1       ; 4
cpu_d6              so.l    1       ; 4
cpu_d7              so.l    1       ; 4
cpu_a0              so.l    1       ; 4
cpu_a1              so.l    1       ; 4
cpu_a2              so.l    1       ; 4
cpu_a3              so.l    1       ; 4
cpu_a4              so.l    1       ; 4
cpu_a5              so.l    1       ; 4
cpu_a6              so.l    1       ; 4
cpu_a7              so.l    1       ; 4
cpu_usp             so.l    1       ; 4
cpu_pc              so.l    1       ; 4
cpu_sr              so.w    1       ; 2
cpu_padding         so.w    1       ; 2
cpu_fsave           so.l    54      ; 216
cpu_fp0             so.l    3       ; 12
cpu_fp1             so.l    3       ; 12
cpu_fp2             so.l    3       ; 12
cpu_fp3             so.l    3       ; 12
cpu_fp4             so.l    3       ; 12
cpu_fp5             so.l    3       ; 12
cpu_fp6             so.l    3       ; 12
cpu_fp7             so.l    3       ; 12
cpu_fpcr            so.l    1       ; 4
cpu_fpsr            so.l    1       ; 4
cpu_fpiar           so.l    1       ; 4

cpu_SIZEOF         so
    ifeq (cpu_SIZEOF == 400)
        fail "CpuContext structure size is incorrect."
    endif


; The VirtualProcessor
kVirtualProcessorState_Ready        equ 0   ; VP is able to run but currently sitting on the ready queue
kVirtualProcessorState_Running      equ 1   ; VP is running
kVirtualProcessorState_Waiting      equ 2   ; VP is blocked waiting for a resource (eg sleep, mutex, semaphore, etc)

    clrso
vp_rewa_queue_entry_next                so.l    1           ; 4
vp_rewa_queue_entry_prev                so.l    1           ; 4
vp_vtable                               so.l    1           ; 4
vp_save_area                            so.b    cpu_SIZEOF  ; 400
vp_kernel_stack_base                    so.l    1           ; 4
vp_kernel_stack_size                    so.l    1           ; 4
vp_user_stack_base                      so.l    1           ; 4
vp_user_stack_size                      so.l    1           ; 4
vp_vpid                                 so.l    1           ; 4
vp_owner_queue_entry_next               so.l    1           ; 4
vp_owner_queue_entry_prev               so.l    1           ; 4
vp_owner_self                           so.l    1           ; 4
vp_syscall_entry_ksp                    so.l    1           ; 4
vp_syscall_ret_0                        so.l    1           ; 4
vp_timeout_queue_entry_next             so.l    1           ; 4
vp_timeout_queue_entry_prev             so.l    1           ; 4
vp_timeout_deadline                     so.l    1           ; 4
vp_timeout_owner                        so.l    1           ; 4
vp_timeout_is_valid                     so.b    1           ; 1
vp_timeout_reserved                     so.b    3           ; 3
vp_waiting_on_wait_queue                so.l    1           ; 4
vp_wait_start_time                      so.l    1           ; 4
vp_wakeup_reason                        so.b    1           ; 1
vp_priority                             so.b    1           ; 1
vp_effectivePriority                    so.b    1           ; 1
vp_state                                so.b    1           ; 1
vp_flags                                so.b    1           ; 1
vp_quantum_allowance                    so.b    1           ; 1
vp_suspension_count                     so.b    1           ; 1
vp_reserved                             so.b    1           ; 1
vp_dispatchQueue                        so.l    1           ; 4
vp_dispatchQueueConcurrencyLaneIndex    so.b    1           ; 1
vp_reserved2                            so.b    3           ; 3
vp_SIZEOF                       so
    ifeq (vp_SIZEOF == 496)
        fail "VirtualProcessor structure size is incorrect."
    endif


    clrso
irc_handlers_SERIAL_TRANSMIT_BUFFER_EMPTY   so.l    2   ; 192
irc_handlers_DISK_BLOCK                     so.l    2
irc_handlers_SOFT                           so.l    2
irc_handlers_PORTS                          so.l    2
irc_handlers_COPPER                         so.l    2
irc_handlers_VERTICAL_BLANK                 so.l    2
irc_handlers_BLITTER                        so.l    2
irc_handlers_AUDIO0                         so.l    2
irc_handlers_AUDIO1                         so.l    2
irc_handlers_AUDIO2                         so.l    2
irc_handlers_AUDIO3                         so.l    2
irc_handlers_SERIAL_RECEIVE_BUFFER_FULL     so.l    2
irc_handlers_DISK_SYNC                      so.l    2
irc_handlers_EXTERN                         so.l    2
irc_handlers_CIA_A_TIMER_A                  so.l    2
irc_handlers_CIA_A_TIMER_B                  so.l    2
irc_handlers_CIA_A_ALARM                    so.l    2
irc_handlers_CIA_A_SP                       so.l    2
irc_handlers_CIA_A_FLAG                     so.l    2
irc_handlers_CIA_B_TIMER_A                  so.l    2
irc_handlers_CIA_B_TIMER_B                  so.l    2
irc_handlers_CIA_B_ALARM                    so.l    2
irc_handlers_CIA_B_SP                       so.l    2
irc_handlers_CIA_B_FLAG                     so.l    2
irc_nextAvailableId                         so.l    1       ; 4
irc_spuriousInterruptCount                  so.l    1       ; 4
irc_uninitializedInterruptCount             so.l    1       ; 4
irc_lock                                    so.l    4       ; 16
irc_SIZEOF                                  so
    ifeq (irc_SIZEOF == 220)
        fail "InterruptController structure size is incorrect."
    endif


COPB_SCHEDULED  equ 7
COPB_INTERLACED equ 6

    clrso
cop_flags                       so.l    1       ; 4
cop_scheduled_prog_odd_field    so.l    1       ; 4
cop_scheduled_prog_even_field   so.l    1       ; 4
cop_scheduled_prog_id           so.l    1       ; 4
cop_running_prog_odd_field      so.l    1       ; 4
cop_running_prog_even_field     so.l    1       ; 4
cop_running_prog_id             so.l    1       ; 4
cop_SIZEOF                      so
    ifeq (cop_SIZEOF == 28)
        fail "Copper structure size is incorrect."
    endif


;-------------------------------------------------------------------------------
; Macros to enable / disable preemption by the virtual processor scheduler
;
; Usage:
;
; DISABLE_PREEMPTION d7
; < do stuff>
; RESTORE_PREEMPTION d7
;

; Disable context switches triggered by preemption (IRQs and quantum interrupts).
; This will stop IRQs from being serviced which turns off scheduling and context
; switching in interrupt contexts until they are reenabled.
; Returns the previous state of the status register.
    macro DISABLE_PREEMPTION
    move.w  sr, \1
    or.w    #$0700, sr
    endm

; Sets the current preemption state based on the saved state in a register
    macro RESTORE_PREEMPTION
    move.w  \1, sr
    endm

; Disable voluntary context switches. These are context switches which are triggered
; by a call to wakeup()
    macro DISABLE_COOPERATION
    bclr    #SCHED_FLAG_VOLUNTARY_CSW_ENABLED, _gVirtualProcessorSchedulerStorage + vps_flags
    sne     \1
    endm

; Restores the given cooperation state. Voluntary context switches are reenabled if
; they were enabled before and disabled otherwise
    macro   RESTORE_COOPERATION
    tst.b   \1
    bne.s   .\@rc1
    bclr    #SCHED_FLAG_VOLUNTARY_CSW_ENABLED, _gVirtualProcessorSchedulerStorage + vps_flags
    bra.s   .\@rc2
.\@rc1:
    bset    #SCHED_FLAG_VOLUNTARY_CSW_ENABLED, _gVirtualProcessorSchedulerStorage + vps_flags
.\@rc2:
    endm


;-------------------------------------------------------------------------------
; Macros to enable / disable interrupt handling by the interrupt controller
;
; Usage:
;
; DISABLE_INTERRUPTS d7
; < do stuff>
; RESTORE_INTERRUPTS d7
;
; NOTE: These macros are functionality-wise a super set of the DISABLE_PREEMPTION
; and RESTORE_PREEMPTION macros because they disable ALL interrupts including
; those which do not cause preemptions. The preemption macros only disable
; interrupts which may trigger a preemption.

; Disable interrupt handling.
; Returns the previous state of the status register.
    macro DISABLE_INTERRUPTS
    move.w  sr, \1
    or.w    #$0700, sr
    endm

; Sets the current interrupt handling state based on the saved state in a register
    macro RESTORE_INTERRUPTS
    move.w  \1, sr
    endm

        endif
