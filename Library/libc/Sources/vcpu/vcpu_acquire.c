//
//  vcpu_acquire.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <stdlib.h>
#include <sys/spinlock.h>
#include <kpi/syscall.h>


static _Noreturn __vcpu_start(vcpu_t self)
{
    self->func(self->arg);

    // Make sure that we clean up the user space side of things before we let
    // the vcpu relinquish for good.
    __vcpu_relinquish(self);
    /* NOT REACHED */
}

vcpu_t _Nullable vcpu_acquire(const vcpu_attr_t* _Nonnull attr)
{
    vcpu_t self = calloc(1, sizeof(struct vcpu));
    _vcpu_acquire_attr_t r_attr;

    if (self == NULL) {
        return NULL;
    }

    self->groupid = attr->groupid;
    self->func = attr->func;
    self->arg = attr->arg;


    r_attr.func = (vcpu_func_t)__vcpu_start;
    r_attr.arg = self;
    r_attr.stack_size = attr->stack_size;
    r_attr.groupid = attr->groupid;
    r_attr.sched_params = attr->sched_params;
    r_attr.flags = attr->flags & ~VCPU_ACQUIRE_RESUMED;
    r_attr.data = (intptr_t)self;

    if (_syscall(SC_vcpu_acquire, &r_attr, &self->id) < 0) {
        free(self);
        return NULL;
    }

    spin_lock(&__g_lock);
    deque_add_last(&__g_all_vcpus, &self->qe);
    spin_unlock(&__g_lock);


    if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
        vcpu_resume(self);
    }
    return self;
}
