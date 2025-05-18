;
;  machine/arch/m68k/syscall.s
;  libsystem
;
;  Created by Dietmar Planitzer on 9/2/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    xdef __syscall


; System call.
;
; A system call looks like this:
;
; a0.l: -> pointer to argument list base
; trap #0
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


;-------------------------------------------------------------------------------
; intptr_t _syscall(int scno, ...)
__syscall:
    lea.l   4(sp), a0
    trap    #0
    rts
