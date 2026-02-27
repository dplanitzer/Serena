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
#include <arch/_dmdef.h>
#include <arch/_fenv.h>

__CPP_BEGIN

#ifndef __STDC_VERSION_FENV_H__
#define __STDC_VERSION_FENV_H__ 202311L
#endif


#if __STDC_VERSION_FENV_H__ >= 202311L
typedef unsigned int femode_t;
#endif
typedef unsigned short fexcept_t;


#if __STDC_VERSION_FENV_H__ >= 202311L
extern const femode_t _FeDflMode;
#define FE_DFL_MODE (&_FeDflMode)
#endif


#define FE_DOWNWARD     2
#define FE_TONEAREST    0
#define FE_TOWARDZERO   1
#define FE_UPWARD       3


extern const fenv_t _FeDflEnv;
#define FE_DFL_ENV (&_FeDflEnv)


extern int feclearexcept(int excepts);
extern int fegetexceptflag(fexcept_t* _Nonnull flagp, int excepts);
extern int feraiseexcept(int excepts);
extern int fesetexceptflag(const fexcept_t* _Nonnull flagp, int excepts);
extern int fetestexcept(int excepts);

extern int fegetround(void);
extern int fesetround(int round);

extern int fegetenv(fenv_t* _Nonnull envp);
extern int feholdexcept(fenv_t* _Nonnull envp);
extern int fesetenv(const fenv_t* _Nonnull envp);
extern int feupdateenv(const fenv_t* _Nonnull envp);


#if __BSD_VISIBLE
extern int feenableexcept(int excepts);
extern int fedisableexcept(int excepts);
#endif


#if __STDC_VERSION_FENV_H__ >= 202311L
extern int fesetexcept(int excepts);
extern int fetestexceptflag(const fexcept_t* _Nonnull flagp, int excepts);

extern int fegetmode(femode_t* _Nonnull modep);
extern int fesetmode(const femode_t* _Nonnull modep);
#endif

__CPP_END

#endif /* _FENV_H */
