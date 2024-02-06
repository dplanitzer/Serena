;
;  setjmp_m68k.s
;  Apollo
;
;  Created by Dietmar Planitzer on 8/26/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    xdef _setjmp
    xdef _longjmp


    clrso
; d0 is used for the setjmp() return value, so no need to save it
; note that we do not save the PC in the jmp_buf because the PC is on the
; stack from the setjmp() call anyway. Setjmp() saves the SP as it points to
; the return address and longjmp() restores this SP. It can then simply return
; with a rts. The rts picks up the original PC and the caller code for the
; setjmp then pops the jmp_buf pointer argument off the stack.
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
; XXX add support for saving the FPU user state

cpu_SIZEOF         so
    ifeq (cpu_SIZEOF == 15*4)
        fail "jmp_buf structure size is incorrect."
    endif


;-------------------------------------------------------------------------------
; int setjmp(jmp_buf env)
_setjmp:
    cargs sj_env_ptr.l
    move.l  a0, d0
    move.l  sj_env_ptr(sp), a0

    ; save the integer state
    movem.l d1-d7 / a0-a7, (a0)

    ; save a0
    move.l  d0, cpu_a0(a0)

    ; XXX save the FPU state if there's a FPU

    ; return 0
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; void longjmp(jmp_buf env, int status)
_longjmp:
    cargs lj_env_ptr.l, lj_status.l

    move.l  lj_env_ptr(sp), a0

    ; get the return value and force it to 1 if 0 was passed
    move.l  lj_status(sp), d0
    bne.s   .L1
    moveq.l #1, d0
.L1:

    ; XXX restore the FPU state if there's a FPU

    ; restore the integer state
    movem.l (a0), d1-d7 / a0-a7

    rts
