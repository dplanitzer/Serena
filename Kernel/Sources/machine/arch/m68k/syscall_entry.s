;
;  machine/arch/m68k/syscall_entry.s
;  kernel
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include <machine/lowmem.i>
    include <machine/asm/errno.i>

    xref _gVirtualProcessorSchedulerStorage
    xref __syscall_handler

    xdef _syscall_entry
    xdef _nosyscall_entry


;-------------------------------------------------------------------------------
; Common entry point for system calls (trap #0).
; A system call looks like this:
;
; a0.l: -> pointer to argument list base
;
; d0.l: <- error number
;
; Register a0 holds a pointer to the base of the argument list. Arguments are
; expected to be ordered from left to right (same as the standard C function
; call ABI) and the pointer in a0 points to the left-most argument. So the
; simplest way to pass arguments to a system call is to push them on the user
; stack starting with the right-most argument and ending with the left-most
; argument and to then initialize a0 like this:
;
; move.l sp, a0
;
; If the arguments are first put on the stack and you then call a subroutine
; which does the actual trap #0 to the kernel, then you want to initialize a0
; like this:
;
; lea 4(sp), a0
;
; since the user stack pointer points to the return address on the stack and not
; the system call number.
;
; The system call returns the error code in d0.
;
; There are a couple advantages to this admittedly unusual system call ABI:
; - the kernel does not have to save d0 since it holds the return value
; - it gives the user space more flexibility:
; -- user space can either implement a __syscall() subroutine or inline the system
;    call and the right thing will happen automatically
; -- user space can either push arguments on the stack or point the kernel to a
;    precomputed argument list that is stored somewhere else
; - it allows the kernel to avoid having to copying the arguments to the super
;   user stack 
;
; This top-level system call handler calls the system call handler functions that
; are responsible for handling the individual system calls. These handlers are
; written in C and they receive a pointer to a structure that holds all the
; system call arguments including the system call number. The arguments are
; ordered from left to right:
;
; struct Args {
;    unsigned int   systemCallNumber;
;    // system call specific arguments from left to right
; }
;
_syscall_entry:
    inline
        ; save the user registers (see description above)
        movem.l d1 - d7 / a0 - a6, -(sp)

        move.l  _gVirtualProcessorSchedulerStorage + vps_running, a1

        ; Invoke the system call handler. Returns a result in d0
        move.l  a0, -(sp)
        move.l  a1, -(sp)
        jsr     __syscall_handler
        addq.l  #8, sp

        ; restore the user registers
        movem.l (sp)+, d1 - d7 / a0 - a6

        rte
    einline


_nosyscall_entry:
    inline
        move.l  a1, -(sp)
        move.l  _gVirtualProcessorSchedulerStorage + vps_running, a1
        move.l  #ENOSYS, vp_uerrno(a1)
        moveq.l #-1, d0
        move.l  (sp)+, a1
        rte
    einline
