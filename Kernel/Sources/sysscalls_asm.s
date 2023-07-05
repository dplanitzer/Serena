;
;  syscalls_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include "lowmem.i"
    include "syscalls.i"

    xref _fatalError
    xref __syscall_VirtualProcessor_Sleep
    xref __syscall_VirtualProcessor_Print

    xdef _SystemCallHandler


; System call table
syscall_table_desc:
    dc.l    0
    dc.l    SC_COUNT-1

syscall_table:
    dc.l __syscall_VirtualProcessor_Sleep
    dc.l __syscall_VirtualProcessor_Print


;-------------------------------------------------------------------------------
; Common entry point for system calls.
; A system call looks like this:
; d0.l: -> system call number
; d0.l: <- error number
; -> arguments are passed on the stack from right to left (like for C functions)
; trap #0
_SystemCallHandler:
    inline
        ; save the ksp at syscall entry (needed by the async user closure invocation system)
        move.l  SCHEDULER_BASE + vps_running, a0
        move.l  sp, vp_syscall_entry_ksp(a0)

        ; note that we do not save the non-volatile registers here because the syscall routines
        ; save the registers which they are really using
;        movem.l d2 - d7 / a2 - a6, -(sp)

        ; Range check the system call number
        cmp2.l  syscall_table_desc(pc), d0
        bcs.s   InvalidSystemCall

        ; Get the system call entry point and jump there
        lea     syscall_table(pc), a0
        move.l  (a0, d0.l*4), a0
        jsr     (a0)

;        movem.l (sp)+, d2 - d7 / a2 - a6
        rte
    einline


InvalidSystemCall:
    pea     0
    pea     .invsyscall_errmsg
    jsr     _fatalError
    ; NOT REACHED

.invsyscall_errmsg:
    dc.b    "Invalid syscall", 0
    align   2
