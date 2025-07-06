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
#include <stdint.h>
#include <kpi/signal.h>
#include <kpi/types.h>
#include <kpi/vcpu.h>

__CPP_BEGIN

extern vcpuid_t vcpu_getid(void);

extern sigset_t vcp_sigmask(void);
extern int vcpu_setsigmask(int op, sigset_t mask, sigset_t* _Nullable oldmask);

extern intptr_t __vcpu_getdata(void);
extern void __vcpu_setdata(intptr_t data);

__CPP_END

#endif /* _SYS_VCPU_H */
