//
//  machine/vcpu_state.h
//  kpi
//
//  Created by Dietmar Planitzer on 4/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _MACHINE_VCPU_STATE_H
#define _MACHINE_VCPU_STATE_H 1

#ifdef __M68K__
#include <machine/m68k/vcpu_state.h>
#else
#error "unknown CPU architecture"
#endif

#endif /* _MACHINE_VCPU_STATE_H */
