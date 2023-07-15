;
;  syscalls_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include "lowmem.i"
    include "syscalls.i"

    xref __syscall_write
    xref __syscall_sleep
    xref __syscall_dispatchAsync
    xref __syscall_exit

    xdef _SystemCallHandler


; Error codes
ENOSYS  equ 15


; System call table
syscall_table_desc:
    dc.l    0
    dc.l    SC_numberOfCalls-1

syscall_table:
    dc.l __syscall_write
    dc.l __syscall_sleep
    dc.l __syscall_dispatchAsync
    dc.l __syscall_exit


;-------------------------------------------------------------------------------
; Common entry point for system calls.
; A system call looks like this:
; d0.l: -> system call number
; d0.l: <- error number
; d1.l to d7.l: -> system call arguments
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
;
; trap #0
_SystemCallHandler:
    inline
        ; save the user registers (see stack layout description above)
        movem.l d1 - d7 / a0 - a6, -(sp)

        ; Range check the system call number
        cmp2.l  syscall_table_desc(pc), d0
        bcs.s   .Linvalid_syscall

        ; save the ksp as it was at syscall entry (needed to be able to abort call-as-user invocations)
        move.l  SCHEDULER_BASE + vps_running, a0
        lea     (14*4, sp), a1                      ; ksp at trap handler entry time was 14 long words higher up in memory
        move.l  a1, vp_syscall_entry_ksp(a0)

        ; Get the system call entry point and jump there
        lea     syscall_table(pc), a0
        move.l  (a0, d0.l*4), a0
        jsr     (a0)

.Lsyscall_done:
        ; restore the user registers
        movem.l (sp)+, d1 - d7 / a0 - a6

        rte

.Linvalid_syscall:
        move.l  #ENOSYS, d0
        bra.s   .Lsyscall_done
    einline
