//
//  machine/_fenv.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _MACHINE_FENV_H
#define _MACHINE_FENV_H 1

#ifdef __M68K__
#include <machine/m68k/_fenv.h>
#else
#error "unknown CPU architecture"
#endif

#endif /* _MACHINE_FENV_H */
