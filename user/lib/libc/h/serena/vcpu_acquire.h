//
//  vcpu_acquire.h
//  libc
//
//  Created by Dietmar Planitzer on 5/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_VCPU_ACQUIRE_H
#define _SERENA_VCPU_ACQUIRE_H 1

#include <_cmndef.h>
#include <stdbool.h>
#include <kpi/vcpu.h>

__CPP_BEGIN

// Initializes a vcpu acquisition attribute object such that a vcp will be
// acquired with the following properties:
// - Assigned to the default vcpu group
// - Utility QoS with Normal priority
// - Start out resumed
extern int vcpu_attr_init(vcpu_attr_t* _Nonnull attr);
extern int vcpu_attr_destroy(vcpu_attr_t* _Nullable attr);

// Sets the function that should be executed by the vcpu and its argument.

// Sets the group to which the vcpu should be assigned.
extern vcpuid_t vcpu_attr_groupid(const vcpu_attr_t* _Nonnull attr);
extern int vcpu_attr_setgroupid(vcpu_attr_t* _Nonnull attr, vcpuid_t id);

// The new vcpu should start out in suspended state and the caller will resume
// it by calling vcpu_resume() when it is ready to do so.
extern bool vcpu_attr_suspended(const vcpu_attr_t* _Nonnull attr);
extern void vcpu_attr_setsuspended(vcpu_attr_t* _Nonnull attr, bool flag);

// Sets the scheduling policy to the provided QoS level of the vcpu.
extern void vcpu_attr_qos(vcpu_attr_t* _Nonnull attr, int* _Nullable qos, int* _Nullable priority);
extern int vcpu_attr_setqos(vcpu_attr_t* _Nonnull attr, int qos, int priority);


// Acquires a vcpu. 'attr' specifies various attributes and how the vcpu should
// be acquired. Returns the id of the newly acquired vcpu on success and -1 if
// acquisition has failed. Each vcpu has a unique id and may be assigned a group
// id. Note that you should use the new_vcpu_groupid() function to generate a
// unique group id to ensure that your group id will not clash with the group
// id that some other library wants to use.
extern vcpu_t _Nullable vcpu_acquire(vcpu_func_t _Nonnull func, void* _Nullable arg, const vcpu_attr_t* _Nonnull attr);

__CPP_END

#endif /* _SERENA_VCPU_ACQUIRE_H */
