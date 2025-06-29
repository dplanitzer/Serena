//
//  sys/vcpu.h
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_VCPU_H
#define _SYS_VCPU_H 1

#include <_cmndef.h>
#include <kpi/vcpu.h>

__CPP_BEGIN

extern int vcpu_self(void);

extern unsigned int vcpu_getsigmask(void);
extern int vcpu_setsigmask(int op, unsigned int mask, unsigned int* _Nullable oldmask);

__CPP_END

#endif /* _SYS_VCPU_H */
