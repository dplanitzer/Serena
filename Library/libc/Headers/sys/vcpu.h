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
#include <stdnoreturn.h>
#include <kpi/signal.h>
#include <kpi/types.h>
#include <kpi/vcpu.h>

__CPP_BEGIN

struct vcpu;
typedef struct vcpu* vcpu_t;

struct vcpu_key;
typedef struct vcpu_key* vcpu_key_t;


// Generates a new process-wide unique vcpu group id
extern vcpuid_t new_vcpu_groupid(void);


extern vcpu_t _Nonnull vcpu_self(void);

extern vcpuid_t vcpu_id(vcpu_t _Nonnull self);
extern vcpuid_t vcpu_groupid(vcpu_t _Nonnull self);

extern sigset_t vcpu_sigmask(void);
extern int vcpu_setsigmask(int op, sigset_t mask, sigset_t* _Nullable oldmask);

extern vcpu_t _Nullable vcpu_acquire(const vcpu_acquire_params_t* _Nonnull params);
extern _Noreturn vcpu_relinquish_self(void);

extern int vcpu_suspend(vcpu_t _Nullable vcpu);
extern void vcpu_resume(vcpu_t _Nonnull vcpu);

extern void vcpu_yield(void);


extern vcpu_key_t _Nullable vcpu_key_create(void (* _Nullable destructor)(void*));
extern void vcpu_key_delete(vcpu_key_t _Nullable key);

extern void *vcpu_specific(vcpu_key_t _Nonnull key);
extern int vcpu_setspecific(vcpu_key_t _Nonnull key, const void* _Nullable value);

__CPP_END

#endif /* _SYS_VCPU_H */
