//
//  vcpu.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "_vcpu.h"
#include <stdlib.h>
#include <sys/spinlock.h>
#include <kpi/syscall.h>

static _Noreturn _vcpu_relinquish(vcpu_t _Nonnull self);


static spinlock_t   g_lock;
static List         g_all_vcpus;
static struct vcpu  g_main_vcpu;


void __vcpu_init(void)
{
    g_lock = SPINLOCK_INIT;
    List_Init(&g_all_vcpus);


    // Init the user space data for the main vcpu
    ListNode_Init(&g_main_vcpu.node);
    g_main_vcpu.id = (vcpuid_t)_syscall(SC_vcpu_getid);
    g_main_vcpu.groupid = 0;
    (void)_syscall(SC_vcpu_setdata, (intptr_t)&g_main_vcpu);


    List_InsertAfterLast(&g_all_vcpus, &g_main_vcpu.node);
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


vcpu_t _Nonnull vcpu_self(void)
{
    return (vcpu_t)_syscall(SC_vcpu_getdata);
}

vcpuid_t vcpu_id(vcpu_t _Nonnull self)
{
    return self->id;
}

vcpuid_t vcpu_groupid(vcpu_t _Nonnull self)
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

int vcpu_suspend(vcpu_t _Nullable vcpu)
{
    return (int)_syscall(SC_vcpu_suspend, (vcpu) ? vcpu->id : VCPUID_SELF);
}

void vcpu_resume(vcpu_t _Nonnull vcpu)
{
    (void)_syscall(SC_vcpu_resume, vcpu->id);
}


static _Noreturn __vcpu_start(vcpu_t self)
{
    self->func(self->arg);

    // Make sure that we clean up the user space side of things before we let
    // the vcpu relinquish for good.
    _vcpu_relinquish(self);
    /* NOT REACHED */
}

vcpu_t _Nullable vcpu_acquire(const vcpu_acquire_params_t* _Nonnull params)
{
    vcpu_t self = calloc(1, sizeof(struct vcpu));
    vcpu_acquire_params_t r_params;

    if (self == NULL) {
        return NULL;
    }

    self->groupid = params->groupid;
    self->func = params->func;
    self->arg = params->arg;


    r_params = *params;
    r_params.func = (vcpu_start_t)__vcpu_start;
    r_params.arg = self;
    r_params.flags = params->flags & ~VCPU_ACQUIRE_RESUMED;

    if (_syscall(SC_vcpu_acquire, &r_params, &self->id) < 0) {
        free(self);
        return NULL;
    }

    spin_lock(&g_lock);
    List_InsertAfterLast(&g_all_vcpus, &self->node);
    spin_unlock(&g_lock);


    if ((params->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
        vcpu_resume(self);
    }
    return self;
}

static _Noreturn _vcpu_relinquish(vcpu_t _Nonnull self)
{
    spin_lock(&g_lock);
    List_Remove(&g_all_vcpus, &self->node);
    spin_unlock(&g_lock);

    if (self != &g_main_vcpu) {
        free(self);
    }

    (_Noreturn)_syscall(SC_vcpu_relinquish_self);
    /* NOT REACHED */
}

_Noreturn vcpu_relinquish_self(void)
{
    _vcpu_relinquish(vcpu_self());
    /* NOT REACHED */
}
