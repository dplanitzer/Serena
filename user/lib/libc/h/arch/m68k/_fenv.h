//
//  m68k/_fenv.h
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _M68K_FENV_H
#define _M68K_FENV_H 1

#define FE_DIVBYZERO    0x04
#define FE_INEXACT      0x02
#define FE_INVALID      0x20
#define FE_OVERFLOW     0x10
#define FE_UNDERFLOW    0x08
#define FE_ALL_EXCEPT   FE_DIVBYZERO | FE_INEXACT | \
                        FE_INVALID | FE_OVERFLOW | \
                        FE_UNDERFLOW

typedef struct fenv {
    unsigned int    __fpcr;
    unsigned int    __fpsr;
} fenv_t;

#endif /* _M68K_FENV_H */
