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

struct vcpu;
typedef struct vcpu* vcpu_t;


// Generates a new process-wide unique vcpu group id
extern vcpuid_t new_vcpu_groupid(void);


extern vcpu_t vcpu_self(void);

extern vcpuid_t vcpu_id(vcpu_t self);
extern vcpuid_t vcpu_groupid(vcpu_t self);


extern sigset_t vcpu_sigmask(void);
extern int vcpu_setsigmask(int op, sigset_t mask, sigset_t* _Nullable oldmask);


extern int vcpu_acquire(const vcpu_acquire_params_t* _Nonnull params, vcpuid_t* _Nonnull idp);
extern void vcpu_relinquish_self(void);

__CPP_END

#endif /* _SYS_VCPU_H */
