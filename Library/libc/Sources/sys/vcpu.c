//
//  vcpu.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "_vcpu.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/spinlock.h>
#include <kpi/syscall.h>

static void _vcpu_destroy_specific(vcpu_t _Nonnull self);
static _Noreturn _vcpu_relinquish(vcpu_t _Nonnull self);


static spinlock_t   g_lock;
static List         g_all_vcpus;
static struct vcpu  g_main_vcpu;
static List         g_vcpu_keys;

// For libdispatch
struct vcpu_key     g_dispatch_key;
vcpu_key_t          __os_dispatch_key;


void __vcpu_init(void)
{
    g_lock = SPINLOCK_INIT;
    g_all_vcpus = LIST_INIT;
    g_vcpu_keys = LIST_INIT;


    // Init the user space data for the main vcpu
    g_main_vcpu.qe = LISTNODE_INIT;
    g_main_vcpu.id = (vcpuid_t)_syscall(SC_vcpu_getid);
    g_main_vcpu.groupid = VCPUID_MAIN_GROUP;
    g_main_vcpu.func = NULL;
    g_main_vcpu.arg = 0;
    g_main_vcpu.specific_tab = NULL;
    g_main_vcpu.specific_capacity = 0;
    (void)_syscall(SC_vcpu_setdata, (intptr_t)&g_main_vcpu);
    List_InsertAfterLast(&g_all_vcpus, &g_main_vcpu.qe);


    // Init the vcpu_key_t for libdispatch. We do it here so that libdispatch
    // can access the key without having to go through a lock (which would be
    // necessary if it would have to allocate it dynamically itself).
    g_dispatch_key.qe = LISTNODE_INIT;
    g_dispatch_key.destructor = NULL;
    __os_dispatch_key = &g_dispatch_key;
    List_InsertAfterLast(&g_vcpu_keys, &g_dispatch_key.qe);
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

vcpu_t _Nonnull vcpu_main(void)
{
    return &g_main_vcpu;
}

vcpuid_t vcpu_id(vcpu_t _Nonnull self)
{
    return self->id;
}

vcpuid_t vcpu_groupid(vcpu_t _Nonnull self)
{
    return self->groupid;
}

int vcpu_suspend(vcpu_t _Nullable vcpu)
{
    return (int)_syscall(SC_vcpu_suspend, (vcpu) ? vcpu->id : VCPUID_SELF);
}

void vcpu_resume(vcpu_t _Nonnull vcpu)
{
    (void)_syscall(SC_vcpu_resume, vcpu->id);
}

void vcpu_yield(void)
{
    (void)_syscall(SC_vcpu_yield);
}

int vcpu_getschedparams(vcpu_t _Nullable vcpu, int type, sched_params_t* _Nonnull params)
{
    return (int)_syscall(SC_vcpu_getschedparams, (vcpu) ? vcpu->id : VCPUID_SELF, type, params);
}

int vcpu_setschedparams(vcpu_t _Nullable vcpu, const sched_params_t* _Nonnull params)
{
    return (int)_syscall(SC_vcpu_setschedparams, (vcpu) ? vcpu->id : VCPUID_SELF, params);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: Acquire/Relinquish

static _Noreturn __vcpu_start(vcpu_t self)
{
    self->func(self->arg);

    // Make sure that we clean up the user space side of things before we let
    // the vcpu relinquish for good.
    _vcpu_relinquish(self);
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

    spin_lock(&g_lock);
    List_InsertAfterLast(&g_all_vcpus, &self->qe);
    spin_unlock(&g_lock);


    if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
        vcpu_resume(self);
    }
    return self;
}

static _Noreturn _vcpu_relinquish(vcpu_t _Nonnull self)
{
    spin_lock(&g_lock);
    List_Remove(&g_all_vcpus, &self->qe);
    spin_unlock(&g_lock);

    if (self != &g_main_vcpu) {
        _vcpu_destroy_specific(self);
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


////////////////////////////////////////////////////////////////////////////////
// MARK: Specific

vcpu_key_t _Nullable vcpu_key_create(vcpu_destructor_t _Nullable destructor)
{
    vcpu_key_t key = malloc(sizeof(struct vcpu_key));

    if (key) {
        key->qe = LISTNODE_INIT;
        key->destructor = destructor;

        spin_lock(&g_lock);
        List_InsertAfterLast(&g_vcpu_keys, &key->qe);
        spin_unlock(&g_lock);
    }
    return key;
}

void vcpu_key_delete(vcpu_key_t _Nullable key)
{
    if (key && key != __os_dispatch_key) {
        spin_lock(&g_lock);
        List_Remove(&g_vcpu_keys, &key->qe);
        spin_unlock(&g_lock);

        free(key);
    }
}

static vcpu_destructor_t _Nullable _vcpu_key_destructor(vcpu_key_t _Nonnull key)
{
    vcpu_destructor_t dstr = NULL;

    spin_lock(&g_lock);
    List_ForEach(&g_vcpu_keys, ListNode, {
        vcpu_key_t cep = (vcpu_key_t)pCurNode;

        if (cep == key) {
            dstr = cep->destructor;
            break; 
        }
    });

    spin_unlock(&g_lock);

    return dstr;
}

static void _vcpu_destroy_specifics(vcpu_specific_t _Nonnull tab, int tabsiz)
{
    for (int i = 0; i < tabsiz; i++) {
        if (tab[i].key) {
            vcpu_destructor_t dstr = _vcpu_key_destructor(tab[i].key);

            if (dstr) {
                dstr(tab[i].value);
            }
        }

        tab[i].key = NULL;
        tab[i].value = NULL;
    }
}

static void _vcpu_destroy_specific(vcpu_t _Nonnull self)
{
    _vcpu_destroy_specifics(self->specific_inline, VCPU_DATA_INLINE_CAPACITY);
    
    if (self->specific_capacity > 0) {
        _vcpu_destroy_specifics(self->specific_tab, self->specific_capacity);
        free(self->specific_tab);
        self->specific_tab = NULL;
    }
}

void *vcpu_specific(vcpu_key_t _Nonnull key)
{
    vcpu_t self = vcpu_self();

    for (int i = 0; i < VCPU_DATA_INLINE_CAPACITY; i++) {
        if (self->specific_inline[i].key == key) {
            return self->specific_inline[i].value;
        }
    }

    for (int i = 0; i < self->specific_capacity; i++) {
        if (self->specific_tab[i].key == key) {
            return self->specific_tab[i].value;
        }
    }

    return NULL;
}

int vcpu_setspecific(vcpu_key_t _Nonnull key, const void* _Nullable value)
{
    vcpu_t self = vcpu_self();
    int availSlotIdx = 0;

    for (int i = 0; i < VCPU_DATA_INLINE_CAPACITY; i++) {
        if (self->specific_inline[i].key == key) {
            self->specific_inline[i].value = value;
            return 0;
        }
    }

    
    for (int i = self->specific_capacity - 1; i >= 0; i--) {
        if (self->specific_tab[i].key != NULL) {
            availSlotIdx = i + 1;
            break;
        }
    }

    if (availSlotIdx >= self->specific_capacity) {
        int new_capacity = self->specific_capacity + VCPU_DATA_ENTRIES_GROW_BY;
        vcpu_specific_t new_tab = realloc(self->specific_tab, new_capacity * sizeof(struct vcpu_specific));

        if (new_tab == NULL) {
            return -1;
        }

        self->specific_tab = new_tab;
        self->specific_capacity = new_capacity;
    }

    self->specific_tab[availSlotIdx].key = key;
    self->specific_tab[availSlotIdx].value = value;

    return 0;
}
