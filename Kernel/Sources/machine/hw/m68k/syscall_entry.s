;
;  machine/hw/m68k/syscall_entry.s
;  kernel
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include <machine/hw/m68k/lowmem.i>
    include <machine/errno.i>

    xref _g_sched
    xref __syscall_handler

    xdef __sys_entry
    xdef __sys_no_entry


;-------------------------------------------------------------------------------
; System call entry point.
;
; NOTE: you are expected to use the _syscall() function to invoke a system call.
; You should not use a trap instruction.
;
; Layout of the user stack when using _syscall():
;
; Arguments are pushed from right to left on the stack. The left-most argument
; is the system call number. The system call result is returned in d0.
;
;
; INTERNAL
;
; Layout of the user stack when using trap #0:
;
; Arguments are pushed from right to left on the stack. The left-most argument
; is a 32bit dummy word. The dummy word is the return address of the _syscall()
; function.
;
; argN
; ...
; arg0
; system call number
; dummy long word / _syscall() return address
; ##### <--- usp
;
__sys_entry:
    inline
        SAVE_CPU_STATE

        GET_CURRENT_VP a1
        move.l  sp, vp_syscall_sa(a1)

        ; Invoke the system call handler. This function writes its function
        ; result to the system call save area
        move.l  usp, a0
        move.l  a0, -(sp)
        move.l  a1, -(sp)
        jsr     __syscall_handler
        addq.l  #8, sp

        RESTORE_CPU_STATE

        rte
    einline


__sys_no_entry:
    inline
        move.l  a1, -(sp)
        GET_CURRENT_VP a1
        move.l  #ENOSYS, vp_uerrno(a1)
        moveq.l #-1, d0
        move.l  (sp)+, a1
        rte
    einline
