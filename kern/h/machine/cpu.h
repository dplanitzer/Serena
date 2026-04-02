//
//  machine/cpu.h
//  kpi
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _MACHINE_CPU_H
#define _MACHINE_CPU_H 1

#ifdef __M68K__
#include <machine/m68k/cpu.h>
#else
#error "unknown CPU architecture"
#endif

#endif /* _MACHINE_CPU_H */
