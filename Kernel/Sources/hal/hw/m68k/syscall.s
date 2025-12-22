;
;  machine/hw/m68k/syscall.s
;  kernel
;
;  Created by Dietmar Planitzer on 9/2/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    xdef __syscall
    xdef _sigurgent
    xdef _sigurgent_end


; System call.
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


;-------------------------------------------------------------------------------
; intptr_t _syscall(int scno, ...)
__syscall:
    trap    #0
    rts


;-------------------------------------------------------------------------------
; void sigurgent(void)
; void sigurgent_end(void)
_sigurgent:
    move.l  #63, -(sp)   ; SC_sigurgent
    subq.l  #4, sp
    trap    #0
    addq.l  #8, sp
    rts
_sigurgent_end:
    nop
