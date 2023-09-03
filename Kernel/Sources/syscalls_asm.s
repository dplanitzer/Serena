;
;  syscalls_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;


    include "lowmem.i"

    xref __SYSCALL_write
    xref __SYSCALL_sleep
    xref __SYSCALL_dispatch_async
    xref __SYSCALL_alloc_address_space
    xref __SYSCALL_exit
    xref __SYSCALL_spawn_process
    xref _gVirtualProcessorSchedulerStorage

    xdef _SystemCallHandler


; System call entry:
; struct _SysCallEntry {
;    void* _Nonnull entry;
;    UInt16         padding;
;    UInt16         numberOfBytesInArgumentList;
; }
; The argument list size should be a multiple of 4 on a 32bit machine and 8 on
; a 64bit machine.

    clrso
sc_entry        so.l    1
sc_padding      so.w    1
sc_nargbytes    so.w    1


syscall_table:
    dc.l __SYSCALL_write, 1*4
    dc.l __SYSCALL_sleep, 2*4
    dc.l __SYSCALL_dispatch_async, 1*4
    dc.l __SYSCALL_alloc_address_space, 2*4
    dc.l __SYSCALL_exit, 1*4
    dc.l __SYSCALL_spawn_process, 1*4

SC_numberOfCalls    equ 6       ; number of system calls


;-------------------------------------------------------------------------------
; Common entry point for system calls (trap #0).
; A system call looks like this:
;
; d0.l: -> system call number
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
; There are a couple advantages to this admitently unusual system call ABI:
; - the kernel does not have to save d0 and a0 since they hold a return and
;   argument value.
; - it gives the user space more flexibility:
; -- user space can either implement a __syscall() subroutine or inline the system
;    call and the right thing will happen automatically
; -- user space can either push arguments on the stack or point the kernel to a
;    precomputed argument list that is stored somewhere else 
;
_SystemCallHandler:
    inline
        ; save the user registers (see description above)
        movem.l d1 - d7 / a1 - a6, -(sp)

        ; save the ksp as it was at syscall entry (needed to be able to abort call-as-user invocations)
        move.l  _gVirtualProcessorSchedulerStorage + vps_running, a1
        lea     (14*4, sp), a2                      ; ksp at trap handler entry time was 14 long words higher up in memory
        move.l  a2, vp_syscall_entry_ksp(a1)

        ; Get the system call number and adjust a0 so that it points to the
        ; left-most (first) actual system call argument
        move.l  (a0)+, d0

        ; Range check the system call number (we treat it as unsigned)
        cmp.l   #SC_numberOfCalls, d0
        bhs.s   .Linvalid_syscall

        ; Get the system call entry structure
        lea     syscall_table(pc), a1
        lea     (a1, d0.l*8), a1

        ; Copy the arguments from the user stack to the superuser stack
        move.w  sc_nargbytes(a1), d7
        beq.s   .L2
        add.w   d7, a0
        move.w  d7, d0
        lsr.w   #2, d0
        subq.w  #1, d0

.L1:    move.l  -(a0), -(sp)
        dbra    d0, .L1

.L2:
        ; Invoke the system call handler
        jsr     ([a1])
        add.w   d7, sp

.Lsyscall_done:

        ; restore the user registers
        movem.l (sp)+, d1 - d7 / a1 - a6

        rte

.Linvalid_syscall:
        move.l  #ENOSYS, d0
        bra.s   .Lsyscall_done
    einline
