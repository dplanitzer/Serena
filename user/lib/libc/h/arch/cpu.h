//
//  arch/cpu.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _ARCH_CPU_H
#define _ARCH_CPU_H 1

#ifdef __M68K__
#include <arch/m68k/cpu.h>
#else
#error "unknown CPU architecture"
#endif

#endif /* _ARCH_CPU_H */
