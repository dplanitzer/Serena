;
;  syscalls_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include "lowmem.i"
    include "syscalls.i"

    xref __SYSCALL_write
    xref __SYSCALL_sleep
    xref __SYSCALL_dispatch_async
    xref __SYSCALL_alloc_address_space
    xref __SYSCALL_exit
    xref __SYSCALL_spawn_process
    xref _gVirtualProcessorSchedulerStorage

    xdef _SystemCallHandler


syscall_table:
    dc.l __SYSCALL_write
    dc.l __SYSCALL_sleep
    dc.l __SYSCALL_dispatch_async
    dc.l __SYSCALL_alloc_address_space
    dc.l __SYSCALL_exit
    dc.l __SYSCALL_spawn_process


;-------------------------------------------------------------------------------
; Common entry point for system calls (trap #0).
; A system call looks like this:
;
; d0.l: -> system call number
; d1.l to d7.l: -> system call arguments
;
; d0.l: <- error number
;
; The system call handler builds a very particular stack frame to ensure that we
; are able to call the system call number specific handler with an argument
; frame that is compatible with C code:
;
; -------------------------------   sp at _SystemCallHandler entry
; |  space for local variables  |
; -------------------------------
; |  saved a6                   |
; |      .                      |
; |      .                      |
; |  saved a0                   |
; -------------------------------
; |  saved d7                   |   right-most syscall argument
; |      .                      |
; |      .                      |
; |  saved d1                   |   left-most syscall argument
; -------------------------------   sp at __syscall_xxx call time
;
; This stack layout makes it possible to call the __syscall_xxx C function in a
; way that is compatible with the C programming language.
;
; Note that we want to save and restore all user space registers anyway because
; we do not want to leak potentially security sensitive information back out to
; user space when we return from the system call.
;
; Note that we do not save & restore d0 because it contains the syscall number
; on entry and the syscall error value result on exit.
; Note that while we write d1 to the kernel stack, we do not restore it because
; it is a parameter on input and a result on output.
;
_SystemCallHandler:
    inline
        ; save the user registers (see stack layout description above)
        movem.l d1 - d7 / a0 - a6, -(sp)

        ; save the ksp as it was at syscall entry (needed to be able to abort call-as-user invocations)
        move.l  _gVirtualProcessorSchedulerStorage + vps_running, a0
        lea     (14*4, sp), a1                      ; ksp at trap handler entry time was 14 long words higher up in memory
        move.l  a1, vp_syscall_entry_ksp(a0)
        move.l  #0, vp_syscall_ret_0(a0)

        ; Range check the system call number (we treat it as unsigned)
        cmp.l   #SC_numberOfCalls, d0
        bhs.s   .Linvalid_syscall

        ; Get the system call entry point and jump there
        lea     syscall_table(pc), a0
        move.l  (a0, d0.l*4), a0
        jsr     (a0)

.Lsyscall_done:
        ; move the syscall result to d1. We always do this. We'll return #0 if
        ; the syscall didn't actually produce any result (see above)
        move.l  _gVirtualProcessorSchedulerStorage + vps_running, a0
        move.l  vp_syscall_ret_0(a0), d1

        ; skip over d1 on the stack since we use d1 as the return-of-result register
        addq.l  #4, sp

        ; restore the user registers
        movem.l (sp)+, d2 - d7 / a0 - a6

        rte

.Linvalid_syscall:
        move.l  #ENOSYS, d0
        bra.s   .Lsyscall_done
    einline
