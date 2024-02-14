//
//  fenv.c
//  libc
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <fenv.h>


// XXX Set this up properly
const fenv_t _DfltFEnv;


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

int feraiseexcept(int excepts)
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

int feupdateenv(const fenv_t* _Nonnull envp)
{
    return -1;
}
