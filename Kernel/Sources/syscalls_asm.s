;
;  syscalls_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include "lowmem.i"
    include <errdef.i>
    include <syscall.i>

    xref __SYSCALL_read
    xref __SYSCALL_write
    xref __SYSCALL_sleep
    xref __SYSCALL_dispatch_async
    xref __SYSCALL_alloc_address_space
    xref __SYSCALL_exit
    xref __SYSCALL_spawn_process
    xref __SYSCALL_getpid
    xref __SYSCALL_getppid
    xref __SYSCALL_getpargs
    xref __SYSCALL_open
    xref __SYSCALL_close
    xref __SYSCALL_waitpid
    xref __SYSCALL_seek
    xref __SYSCALL_getcwd
    xref __SYSCALL_setcwd
    xref __SYSCALL_getuid
    xref __SYSCALL_getumask
    xref __SYSCALL_setumask
    xref __SYSCALL_mkdir
    xref __SYSCALL_getfileinfo
    xref __SYSCALL_opendir
    xref __SYSCALL_setfileinfo
    xref __SYSCALL_access
    xref __SYSCALL_fgetfileinfo
    xref __SYSCALL_fsetfileinfo
    xref __SYSCALL_unlink
    xref _gVirtualProcessorSchedulerStorage

    xdef _SystemCallHandler


syscall_table:
    dc.l __SYSCALL_read
    dc.l __SYSCALL_write
    dc.l __SYSCALL_sleep
    dc.l __SYSCALL_dispatch_async
    dc.l __SYSCALL_alloc_address_space
    dc.l __SYSCALL_exit
    dc.l __SYSCALL_spawn_process
    dc.l __SYSCALL_getpid
    dc.l __SYSCALL_getppid
    dc.l __SYSCALL_getpargs
    dc.l __SYSCALL_open
    dc.l __SYSCALL_close
    dc.l __SYSCALL_waitpid
    dc.l __SYSCALL_seek
    dc.l __SYSCALL_getcwd
    dc.l __SYSCALL_setcwd
    dc.l __SYSCALL_getuid
    dc.l __SYSCALL_getumask
    dc.l __SYSCALL_setumask
    dc.l __SYSCALL_mkdir
    dc.l __SYSCALL_getfileinfo
    dc.l __SYSCALL_opendir
    dc.l __SYSCALL_setfileinfo
    dc.l __SYSCALL_access
    dc.l __SYSCALL_fgetfileinfo
    dc.l __SYSCALL_fsetfileinfo
    dc.l __SYSCALL_unlink


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
; struct _Args {
;    Int  systemCallNumber;
;    // system call specific arguments from left to right
; }
;
_SystemCallHandler:
    inline
        ; save the user registers (see description above)
        movem.l d1 - d7 / a1 - a6, -(sp)

        ; save the ksp as it was at syscall entry (needed to be able to abort call-as-user invocations)
        move.l  _gVirtualProcessorSchedulerStorage + vps_running, a1
        lea     (14*4, sp), a2                      ; ksp at trap handler entry time was 14 long words higher up in memory
        move.l  a2, vp_syscall_entry_ksp(a1)

        ; Get the system call number
        move.l  (a0), d0

        ; Range check the system call number (we treat it as unsigned)
        cmp.l   #SC_numberOfCalls, d0
        bhs.s   .Linvalid_syscall

        ; Get the system call entry structure
        lea     syscall_table(pc), a1
        move.l  (a1, d0.l*4), a1

        ; Invoke the system call handler. Returns a result in d0
        move.l  a0, -(sp)
        jsr     (a1)
        move.l  (sp)+, a0

.Lsyscall_done:

        ; restore the user registers
        movem.l (sp)+, d1 - d7 / a1 - a6

        rte

.Linvalid_syscall:
        move.l  #ENOSYS, d0
        bra.s   .Lsyscall_done
    einline
