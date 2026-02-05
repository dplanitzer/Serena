//
//  arch/_fenv.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _ARCH_FENV_H
#define _ARCH_FENV_H 1

#ifdef __M68K__
#include <arch/m68k/_fenv.h>
#else
#error "unknown CPU architecture"
#endif

#endif /* _ARCH_FENV_H */
