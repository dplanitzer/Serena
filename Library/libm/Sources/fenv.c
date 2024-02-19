//
//  fenv.c
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <fenv.h>


#if !defined(__M68K__)
const femode_t _FeDflMode;
const fenv_t _FeDflEnv;
#endif

#if !defined(__M68K__)
int feclearexcept(int excepts)
{
    if (excepts == 0) {
        return 0;
    }

    return -1;
}

int fegetexceptflag(fexcept_t* _Nonnull flagp, int excepts)
{
    return -1;
}
#endif

int feraiseexcept(int excepts)
{
    // XXX not clear how to best raise exceptions on m68k
    return -1;
}

#if !defined(__M68K__)
int fesetexcept(int excepts)
{
    return -1;
}

int fesetexceptflag(const fexcept_t* _Nonnull flagp, int excepts)
{
    return -1;
}

int fetestexcept(int excepts)
{
    return -1;
}

int fegetmode(femode_t* _Nonnull modep)
{
    return -1;
}

int fesetmode(const femode_t* _Nonnull modep)
{
    return -1;
}

int fegetround(void)
{
    return -1;
}

int fesetround(int round)
{
    return -1;
}

int fegetenv(fenv_t* _Nonnull envp)
{
    return -1;
}

int feholdexcept(fenv_t* _Nonnull envp)
{
    return -1;
}

int fesetenv(const fenv_t* _Nonnull envp)
{
    return -1;
}
#endif

int feupdateenv(const fenv_t* _Nonnull envp)
{
    // XXX not clear how to best raise exceptions on m68k
    return -1;
}


#ifdef __STDC_IEC_60559_DFP__
int fe_dec_getround(void)
{
    return -1;
}

int fe_dec_setround(int rnd)
{
    return -1;
}
#endif
