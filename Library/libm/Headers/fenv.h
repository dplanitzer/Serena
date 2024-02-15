//
//  fenv.h
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _FENV_H
#define _FENV_H 1

#include <System/_cmndef.h>
#include <System/_nulldef.h>
#include <System/abi/_dmdef.h>

__CPP_BEGIN

// XXX This is all just a rough draft. Fix this once we properly initialize the FPU
typedef unsigned int fenv_t;
typedef unsigned int fexcept_t;

#define FE_DIVBYZERO    1
#define FE_INEXACT      2
#define FE_INVALID      4
#define FE_OVERFLOW     8
#define FE_UNDERFLOW    16
#define FE_ALL_EXCEPT   FE_DIVBYZERO | FE_INEXACT | \
                        FE_INVALID | FE_OVERFLOW | \
                        FE_UNDERFLOW

#define FE_DOWNWARD     3
#define FE_TONEAREST    1
#define FE_TOWARDZERO   0
#define FE_UPWARD       2

extern const fenv_t _DfltFEnv;
#define FE_DFL_ENV (&_DfltFEnv)


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

__CPP_END

#endif /* _FENV_H */
