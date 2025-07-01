//
//  vcpu.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/vcpu.h>
#include <kpi/syscall.h>
#include "_vcpu.h"


vcpuid_t vcpu_self(void)
{
    return (vcpuid_t)_syscall(SC_vcpu_self);
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
    self->id = vcpu_self();
    __vcpu_setdata((intptr_t) self);
}

vcpu_t* _Nonnull __vcpu_self(void)
{
    return (vcpu_t*)__vcpu_getdata();
}
