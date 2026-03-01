;
;  arch/m68k/setjmp.s
;  libc
;
;  Created by Dietmar Planitzer on 8/26/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    xdef _setjmp
    xdef _longjmp


    clrso
; d0 is used for the setjmp() return value, so no need to save it.
; Note that we do not save volatile registers because the caller is required to
; save them anyway before doing a function call, if it depends on their values.
;
; XXX should obfuscate the values in this by XORing them with a per vcpu value.
; XXX E.g. a value provided by the kernel and stored in a reserved register or so 
cpu_d2              so.l    1       ; 4
cpu_d3              so.l    1       ; 4
cpu_d4              so.l    1       ; 4
cpu_d5              so.l    1       ; 4
cpu_d6              so.l    1       ; 4
cpu_d7              so.l    1       ; 4
cpu_a2              so.l    1       ; 4
cpu_a3              so.l    1       ; 4
cpu_a4              so.l    1       ; 4
cpu_a5              so.l    1       ; 4
cpu_a6              so.l    1       ; 4
cpu_a7              so.l    1       ; 4
cpu_pc              so.l    1       ; 4

cpu_SIZEOF         so
    ifeq (cpu_SIZEOF == 13*4)
        fail "jmp_buf struct size is incorrect."
    endif


;-------------------------------------------------------------------------------
; int setjmp(jmp_buf env)
_setjmp:
    cargs sj_env_ptr.l

    move.l  sj_env_ptr(sp), a0

    ; save the integer state
    movem.l d2-d7 / a2-a7, (a0)

    ; save PC
    move.l      0(sp), a1
    move.l      a1, cpu_pc(a0)

    ; return 0
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; void longjmp(jmp_buf env, int val)
_longjmp:
    cargs lj_env_ptr.l, lj_val.l

    move.l  lj_env_ptr(sp), a0

    ; get the return value and force it to 1 if 0 was passed
    move.l  lj_val(sp), d0
    bne.s   .1
    moveq.l #1, d0
.1:

    ; restore the integer state
    movem.l (a0), d2-d7 / a2-a7

    ; jump to the save PC
    move.l  cpu_pc(a0), a1
    jmp     (a1)
