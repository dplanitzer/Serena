;
;  fenv.s
;  libm
;
;  Created by Dietmar Planitzer on 2/18/24.
;  Copyright © 2024 Dietmar Planitzer. All rights reserved.
;

    xdef _feclearexcept
    xdef _feraiseexcept
    xdef _fegetexceptflag
    xdef _fesetexceptflag
    xdef _fetestexcept

    xdef _fegetround
    xdef _fesetround

    xdef _fegetenv
    xdef _feholdexcept
    xdef _fesetenv
    xdef _feupdateenv

    xdef _fesetexcept
    xdef _fetestexceptflag

    xdef _fegetmode
    xdef _fesetmode

    xdef _feenableexcept
    xdef _fedisableexcept

    xdef __FeDflMode
    xdef __FeDflEnv


FE_DIVBYZERO    equ     $4
FE_INEXACT      equ     $2
FE_INVALID      equ     $20
FE_OVERFLOW     equ     $10
FE_UNDERFLOW    equ     $8
_FE_INEX1       equ     $1
_FE_SNAN        equ     $40
_FE_BSUN        equ     $80

FE_ALL_EXCEPT   equ     (FE_DIVBYZERO | FE_INEXACT | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)
_FE_ALL_EXCEPT  equ     (FE_ALL_EXCEPT | _FE_INEX1 | _FE_SNAN | _FE_BSUN)

FE_DOWNWARD         equ 2
FE_TONEAREST        equ 0
FE_TOWARDZERO       equ 1
FE_UPWARD           equ 3
FE_NUMROUNDINGMODES equ 4
FE_ALL_ROUND    equ     (FE_DOWNWARD | FE_TONEAREST | FE_TOWARDZERO | FE_UPWARD)


;-------------------------------------------------------------------------------
; int feclearexcept(int excepts)
_feclearexcept:
    cargs fece_excepts.l

    move.l  fece_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    cmp.b   #FE_ALL_EXCEPT, d0
    bne.s   .1

    ; clear all exceptions including 'internal' exception bits
    fmove.l fpsr, d0
    clr.w   d0
    fmove.l d0, fpsr
    moveq.l #0, d0
    rts

.1:
    ; clear specific exceptions
    not.b   d0
    bfins   d0, d1{24:8}        ; clear accrued and non-accrued exceptions
    bfins   d0, d1{16:8}
    fmove.l fpsr, d0
    and.w   d1, d0
    fmove.l d0, fpsr
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int feraiseexcept(int excepts)
_feraiseexcept:
    cargs fere_excepts.l

    move.l  fere_excepts(sp), d0
    move.l  d0, -(sp)
    move.l  d0, -(sp)
    bsr.s   _fesetexceptflag    ; fesetexcptflag(&copy_of_excepts, excepts)
    addq.l  #8, sp
    rts


;-------------------------------------------------------------------------------
; int fegetexceptflag(fexcept_t* _Nonnull flagp, int excepts)
_fegetexceptflag:
    cargs fegef_flagp.l, fegef_excepts.l

    move.l  fegef_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    move.l  fegef_flagp(sp), a0

    fmove.l fpsr, d1
    and.w   d0, d1          ; get accrued exceptions
    move.w  d1, (a0)

    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; fesetexceptflag(const fexcept_t* _Nonnull flagp, int excepts)
_fesetexceptflag:
    cargs fesef_flagp.l, fesef_excepts.l

    move.l  fesef_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    move.l  d2, -(sp)

    not.b   d0
    bfins   d0, d1{24:8}
    bfins   d0, d1{16:8}

    fmove.l fpsr, d2
    and.w   d1, d2          ; fpsr &= ~excepts for exception and accrued exception bits

    not.b   d0              ; get back the original excepts
    and.w   fesef_flagp(sp), d0     ; d0.w = *flagp & excepts
    bfins   d0, d1{24:8}            ; set accrued and non-accrued exceptions
    bfins   d0, d1{16:8}
    or.w    d1, d2          ; fpsr |= d0.w
    fmove.l d2, fpsr
    
    move.l  (sp)+, d2
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fetestexcept(int excepts)
_fetestexcept:
    cargs fete_excepts.l

    move.l  fete_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    fmove.l fpsr, d1
    and.l   d1, d0          ; test accrued exceptions
    rts


;-------------------------------------------------------------------------------
; int fegetround(void)
_fegetround:
    fmove.l fpcr, d0
    lsr.b   #4, d0
    and.l   #FE_ALL_ROUND, d0
    rts


;-------------------------------------------------------------------------------
; int fesetround(int round)
_fesetround:
    cargs fesr_round.l

    move.l  fesr_round(sp), d0
    cmp     #FE_NUMROUNDINGMODES, d0
    bhs.s   .1

    fmove.l fpcr, d1
    bfins   d0, d1{26:2}
    fmove.l d1, fpcr
    moveq.l #0, d0
    rts

.1:
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
    and.w   #$0000, d1      ; clear accrued and non-accrued exceptions
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


;-------------------------------------------------------------------------------
; int feupdateenv(const fenv_t* _Nonnull envp)
_feupdateenv:
    cargs feue_envp.l

    move.l  feue_envp(sp), a0

    move.l  d2, -(sp)
    fmove.l fpsr, d2

    move.l  a0, -(sp)
    bsr.s   _fesetenv       ; fesetenv(envp)
    addq.l  #4, sp

    and.l   #FE_ALL_EXCEPT, d2
    move.l  d2, -(sp)
    jsr     _feraiseexcept  ; feraiseexcept(fpsr.accrued_except & FE_ALL_EXCEPT)
    addq.l  #4, sp

    move.l  (sp)+, d2
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fesetexcept(int excepts)
_fesetexcept:
    cargs fese_excepts.l

    move.l  fese_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    
    bfins   d0, d1{24:8}            ; set accrued and non-accrued exceptions
    bfins   d0, d1{16:8}

    fmove.l fpsr, d0
    or.w    d1, d0
    fmove.l d0, fpsr

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
    rts


;-------------------------------------------------------------------------------
; int fegetmode(femode_t* _Nonnull modep)
_fegetmode:
    cargs fegm_modep.l

    move.l  fegm_modep(sp), a0
    fmove.l fpcr, d0
    and.l   #$00ff, d0
    move.l  d0, (a0)
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int fesetmode(femode_t* _Nonnull modep)
_fesetmode:
    cargs fesm_modep.l

    move.l  fesm_modep(sp), a0
    move.l  (a0), d0
    and.l   #$00ff, d0
    fmove.l fpcr, d1
    bfins   d0, d1{24:8}
    fmove.l d1, fpcr
    moveq.l #0, d0
    rts


;-------------------------------------------------------------------------------
; int feenableexcept(int excepts)
_feenableexcept:
    cargs feee_excepts.l

    move.l  feee_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    
    move.l  d2, -(sp)
    fmove.l fpcr, d1
    move.l  d1, d2
    lsl.l   #8, d0
    or.w    d0, d1
    fmove.l d1, fpcr

    lsr.l   #8, d2
    and.l   #FE_ALL_EXCEPT, d2
    move.l  d2, d0

    move.l  (sp)+, d2
    rts


;-------------------------------------------------------------------------------
; int fedisableexcept(int excepts)
_fedisableexcept:
    cargs fede_excepts.l

    move.l  fede_excepts(sp), d0
    and.l   #FE_ALL_EXCEPT, d0
    not.l   d0
    
    move.l  d2, -(sp)
    fmove.l fpcr, d1
    move.l  d1, d2
    lsl.l   #8, d0
    and.l   d0, d1
    fmove.l d1, fpcr

    lsr.l   #8, d2
    and.l   #FE_ALL_EXCEPT, d2
    move.l  d2, d0

    move.l  (sp)+, d2
    rts


    data

__FeDflMode:
    dc.l 0

__FeDflEnv:
    dc.l    0, 0
