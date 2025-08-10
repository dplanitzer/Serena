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
#include <kpi/types.h>
#include <kpi/vcpu.h>

__CPP_BEGIN

struct vcpu_key;
typedef struct vcpu_key* vcpu_key_t;


typedef struct vcpu_attr {
    int                     version;                // Version 0
    vcpu_func_t _Nullable   func;
    void* _Nullable         arg;
    size_t                  stack_size;
    vcpuid_t                groupid;
    vcpu_sched_params_t     sched_params;
    unsigned int            flags;
} vcpu_attr_t;

#define VCPU_ATTR_INIT  {0}


// Generates a new process-wide unique vcpu group id
extern vcpuid_t new_vcpu_groupid(void);


// Returns the identity of the vcpu on which the caller is executing.
extern vcpu_t _Nonnull vcpu_self(void);

// Returns the identity of the main vcpu of the calling process. The main vcpu
// of a process is the first vcpu that got attached to the process.
extern vcpu_t _Nonnull vcpu_main(void);


extern vcpuid_t vcpu_id(vcpu_t _Nonnull self);
extern vcpuid_t vcpu_groupid(vcpu_t _Nonnull self);

// Acquires a vcpu. 'attr' specifies various attributes and how the vcpu should
// be acquired. Returns the id of the newly acquired vcpu on success and -1 if
// acquisition has failed. Each vcpu has a unique id and may be assigned a group
// id. Note that you should use the new_vcpu_groupid() function to generate a
// unique group id to ensure that your group id will not clash with the group
// id that some other library wants to use.
extern vcpu_t _Nullable vcpu_acquire(const vcpu_attr_t* _Nonnull attr);

// Relinquishes the vcpu on which this call is executed back to the system and
// makes it available for reuse. This is teh same as returning from the vcpu
// top-level function invocation.
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
