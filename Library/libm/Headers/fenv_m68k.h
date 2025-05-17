//
//  fenv_m68k.h
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _FENV_H
#define _FENV_H 1

#include <_cmndef.h>
#include <machine/abi/_dmdef.h>

__CPP_BEGIN

#ifndef __STDC_VERSION_FENV_H__
#define __STDC_VERSION_FENV_H__ 202311L
#endif


typedef unsigned int femode_t;
typedef unsigned int fexcept_t;

typedef struct {
    unsigned int    __fpcr;
    unsigned int    __fpsr;
} fenv_t;


extern const femode_t _FeDflMode;
#define FE_DFL_MODE (&_FeDflMode)


#define FE_DIVBYZERO    0x10
#define FE_INEXACT      0x08
#define FE_INVALID      0x80
#define FE_OVERFLOW     0x40
#define FE_UNDERFLOW    0x20
#define FE_ALL_EXCEPT   FE_DIVBYZERO | FE_INEXACT | \
                        FE_INVALID | FE_OVERFLOW | \
                        FE_UNDERFLOW

#define FE_DOWNWARD     2
#define FE_TONEAREST    0
#define FE_TOWARDZERO   1
#define FE_UPWARD       3

extern const fenv_t _FeDflEnv;
#define FE_DFL_ENV (&_FeDflEnv)


extern int feclearexcept(int excepts);
extern int fegetexceptflag(fexcept_t* _Nonnull flagp, int excepts);
extern int feraiseexcept(int excepts);
extern int fesetexcept(int excepts);
extern int fesetexceptflag(const fexcept_t* _Nonnull flagp, int excepts);
extern int fetestexceptflag(const fexcept_t* _Nonnull flagp, int excepts);
extern int fetestexcept(int excepts);

extern int fegetmode(femode_t* _Nonnull modep);
extern int fesetmode(const femode_t* _Nonnull modep);

extern int fegetround(void);
extern int fesetround(int round);

extern int fegetenv(fenv_t* _Nonnull envp);
extern int feholdexcept(fenv_t* _Nonnull envp);
extern int fesetenv(const fenv_t* _Nonnull envp);
extern int feupdateenv(const fenv_t* _Nonnull envp);

#ifdef __STDC_IEC_60559_DFP__
extern int fe_dec_getround(void);
extern int fe_dec_setround(int rnd);
#endif

__CPP_END

#endif /* _FENV_H */
