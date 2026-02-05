//
//  m68k/_fenv.h
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _M68K_FENV_H
#define _M68K_FENV_H 1

typedef struct fenv {
    unsigned int    __fpcr;
    unsigned int    __fpsr;
} fenv_t;

#endif /* _M68K_FENV_H */
