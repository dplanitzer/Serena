//
//  vcpu.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "_vcpu.h"
#include <sys/spinlock.h>
#include <kpi/syscall.h>


int vcpu_acquire(const vcpu_acquire_params_t* _Nonnull params, vcpuid_t* _Nonnull idp)
{
    return (int) _syscall(SC_vcpu_acquire, params, idp);
}

void vcpu_relinquish_self(void)
{
    (void)_syscall(SC_vcpu_relinquish_self);
}


SList       g_all_vcpus;
struct vcpu g_main_vcpu;

void __vcpu_init(void)
{
    SList_Init(&g_all_vcpus);
    vcpu_init(&g_main_vcpu, 0);
    SList_InsertAfterLast(&g_all_vcpus, &g_main_vcpu.node);
}


void vcpu_init(vcpu_t _Nonnull self, vcpuid_t groupid)
{
    SListNode_Init(&self->node);
    self->id = (vcpuid_t)_syscall(SC_vcpu_getid);
    self->groupid = groupid;

    (void)_syscall(SC_vcpu_setdata, (intptr_t)self);
}


vcpuid_t new_vcpu_groupid(void)
{
    static spinlock_t l;
    static vcpuid_t id;

    spin_lock(&l);
    const newid = ++id;
    spin_unlock(&l);

    return newid;
}


vcpu_t vcpu_self(void)
{
    return (vcpu_t)_syscall(SC_vcpu_getdata);
}

vcpuid_t vcpu_id(vcpu_t self)
{
    return self->id;
}

vcpuid_t vcpu_groupid(vcpu_t self)
{
    return self->groupid;
}

sigset_t vcpu_sigmask(void)
{
    sigset_t mask;

    (void)_syscall(SC_vcpu_setsigmask, SIG_BLOCK, 0, &mask);
    return mask;
}

int vcpu_setsigmask(int op, sigset_t mask, sigset_t* _Nullable oldmask)
{
    return (int)_syscall(SC_vcpu_setsigmask, op, mask, oldmask);
}
