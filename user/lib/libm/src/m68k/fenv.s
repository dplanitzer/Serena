;
;  fenv.s
;  libm
;
;  Created by Dietmar Planitzer on 2/18/24.
;  Copyright © 2024 Dietmar Planitzer. All rights reserved.
;

    xdef _feclearexcept
    xdef _fegetexceptflag
    xdef _fesetexcept
    xdef _fesetexceptflag
    xdef _fetestexceptflag
    xdef _fetestexcept
    xdef _fegetmode
    xdef _fesetmode
    xdef _fegetround
    xdef _fesetround
    xdef _fegetenv
    xdef _feholdexcept
    xdef _fesetenv
    xdef __FeDflMode
    xdef __FeDflEnv


FE_DIVBYZERO    equ     $10
FE_INEXACT      equ     $8
FE_INVALID      equ     $80
FE_OVERFLOW     equ     $40
FE_UNDERFLOW    equ     $20
FE_ALL_EXCEPT   equ     (FE_DIVBYZERO | FE_INEXACT | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)

FE_DOWNWARD         equ 2
FE_TONEAREST        equ 0
FE_TOWARDZERO       equ 1
FE_UPWARD           equ 3
FE_NUMROUNDINGMODES equ 4


;-------------------------------------------------------------------------------
; int feclearexcept(int excepts)
_feclearexcept:
    cargs fece_excepts.l

    move.l  fece_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    beq.s   .done
    not.l   d0
    fmove.l fpsr, d1
    and.l   d0, d1
    fmove.l d1, fpsr

.done:
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fegetexceptflag(fexcept_t* _Nonnull flagp, int excepts)
_fegetexceptflag:
    cargs fegef_flagp.l, fegef_excepts.l

    move.l  fegef_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    move.l  fegef_flagp(sp), a0
    fmove.l fpsr, d1
    and.l   d0, d1
    move.l  d1, (a0)

    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fesetexcept(int excepts)
_fesetexcept:
    cargs fese_excepts.l

    move.l  fese_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    beq.s   .done
    fmove.l fpsr, d1
    or.l    d0, d1
    fmove.l d1, fpsr

.done:
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; fesetexceptflag(const fexcept_t* _Nonnull flagp, int excepts)
_fesetexceptflag:
    cargs fesef_flagp.l, fesef_excepts.l

    move.l  fesef_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    move.l  fesef_flagp(sp), a0
    move.l  (a0), d1
    and.l   d0, d1
    beq.s   .done
    fmove.l d1, fpsr

.done:
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fetestexceptflag(const fexcept_t* _Nonnull flagp, int excepts)
_fetestexceptflag:
    cargs fetef_flagp.l, fetef_excepts.l

    move.l  fetef_excepts(sp), d0
    move.l  fetef_flagp(sp), a0
    move.l  (a0), d1
    and.l   d1, d0
    and.l   #FE_ALL_EXCEPT, d0
    fmove.l fpsr, d1
    and.l   d1, d0
    rts


;-------------------------------------------------------------------------------
; int fetestexcept(int excepts)
_fetestexcept:
    cargs fete_excepts.l

    move.l  fete_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    fmove.l fpsr, d1
    and.l   d1, d0
    rts


;-------------------------------------------------------------------------------
; int fegetmode(femode_t* _Nonnull modep)
_fegetmode:
    cargs fegm_modep.l

    move.l  fegm_modep(sp), a0
    fmove.l fpcr, d0
    and.l   #$f0, d0
    move.l  d0, (a0)
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fesetmode(femode_t* _Nonnull modep)
_fesetmode:
    cargs fesm_modep.l

    move.l  fesm_modep(sp), a0
    move.l  (a0), d0
    and.l   #$f0, d0
    fmove.l fpcr, d1
    or.l    d0, d1
    fmove.l d1, fpcr
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fegetround(void)
_fegetround:
    fmove.l fpcr, d0
    lsr.b   #4, d0
    and.l   #3, d0
    rts


;-------------------------------------------------------------------------------
; int fesetround(int round)
_fesetround:
    cargs fesr_round.l

    move.l  fesr_round(sp), d0
    cmp     #FE_NUMROUNDINGMODES, d0
    bhs.s   .error

    fmove.l fpcr, d1
    bfins   d0, d1{4:2}
    fmove.l d1, fpcr
    moveq.l #0, d0
    rts

.error:
    moveq.l #-1, d0
    rts


;-------------------------------------------------------------------------------
; int fegetenv(fenv_t* _Nonnull envp)
_fegetenv:
    cargs fege_envp.l

    move.l  fege_envp(sp), a0
    fmove.l fpcr, 0(a0)
    fmove.l fpsr, 4(a0)
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int feholdexcept(fenv_t* _Nonnull envp)
_feholdexcept:
    cargs fehe_envp.l

    move.l  fehe_envp(sp), a0
    fmove.l fpcr, d0
    move.l  d0, 0(a0)
    fmove.l fpsr, d1
    move.l  d1, 4(a0)
    and.w   #$00ff, d0      ; disable all exceptions
    and.w   #$ff00, d1      ; clear accrued exceptions
    fmove.l d0, fpcr
    fmove.l d1, fpsr
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fesetenv(const fenv_t* _Nonnull envp)
_fesetenv:
    cargs fese_envp.l

    move.l  fese_envp(sp), a0
    fmove.l 0(a0), fpcr
    fmove.l 4(a0), fpsr
    moveq.l #0, d0
    rts


    data

__FeDflMode:
    dc.l 0

__FeDflEnv:
    dc.l    0, 0
