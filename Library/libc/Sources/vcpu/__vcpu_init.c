//
//  __vcpu_init.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <sys/spinlock.h>
#include <kpi/syscall.h>


spinlock_t  __g_lock;
List        __g_all_vcpus;
struct vcpu __g_main_vcpu;
List        __g_vcpu_keys;

// For libdispatch
static struct vcpu_key  __g_dispatch_key;
vcpu_key_t              __os_dispatch_key;


void __vcpu_init(void)
{
    __g_lock = SPINLOCK_INIT;
    __g_all_vcpus = LIST_INIT;
    __g_vcpu_keys = LIST_INIT;


    // Init the user space data for the main vcpu
    __g_main_vcpu.qe = LISTNODE_INIT;
    __g_main_vcpu.id = (vcpuid_t)_syscall(SC_vcpu_getid);
    __g_main_vcpu.groupid = VCPUID_MAIN_GROUP;
    __g_main_vcpu.func = NULL;
    __g_main_vcpu.arg = 0;
    __g_main_vcpu.specific_tab = NULL;
    __g_main_vcpu.specific_capacity = 0;
    (void)_syscall(SC_vcpu_setdata, (intptr_t)&__g_main_vcpu);
    List_InsertAfterLast(&__g_all_vcpus, &__g_main_vcpu.qe);


    // Init the vcpu_key_t for libdispatch. We do it here so that libdispatch
    // can access the key without having to go through a lock (which would be
    // necessary if it would have to allocate it dynamically itself).
    __g_dispatch_key.qe = LISTNODE_INIT;
    __g_dispatch_key.destructor = NULL;
    __os_dispatch_key = &__g_dispatch_key;
    List_InsertAfterLast(&__g_vcpu_keys, &__g_dispatch_key.qe);
}
