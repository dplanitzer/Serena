//
//  vcpu.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/vcpu.h>
#include <sys/spinlock.h>
#include <kpi/syscall.h>
#include "_vcpu.h"


vcpuid_t vcpu_getid(void)
{
    return (vcpuid_t)_syscall(SC_vcpu_getid);
}

vcpuid_t vcpu_getgrp(void)
{
    return (vcpuid_t)_syscall(SC_vcpu_getgrp);
}

vcpuid_t vcpu_make_grp(void)
{
    static spinlock_t l;
    static vcpuid_t id;

    spin_lock(&l);
    const newid = ++id;
    spin_unlock(&l);

    return newid;
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

intptr_t __vcpu_getdata(void)
{
    return (intptr_t)_syscall(SC_vcpu_getdata);
}

void __vcpu_setdata(intptr_t data)
{
    (void)_syscall(SC_vcpu_setdata, data);
}


SList gVcpus;
vcpu_t  gMainVcpu;

void __vcpu_init(void)
{
    SList_Init(&gVcpus);
    vcpu_init(&gMainVcpu);
    SList_InsertAfterLast(&gVcpus, &gMainVcpu.node);
}

void vcpu_init(vcpu_t* _Nonnull self)
{
    SListNode_Init(&self->node);
    SListNode_Init(&self->wq_node);
    self->id = vcpu_getid();
    __vcpu_setdata((intptr_t) self);
}

vcpu_t* _Nonnull __vcpu_self(void)
{
    return (vcpu_t*)__vcpu_getdata();
}
