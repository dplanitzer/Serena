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

static void _vcpu_destroy_specific(vcpu_t _Nonnull self);
static _Noreturn _vcpu_relinquish(vcpu_t _Nonnull self);


static spinlock_t   g_lock;
static List         g_all_vcpus;
static struct vcpu  g_main_vcpu;
static List         g_vcpu_keys;


void __vcpu_init(void)
{
    g_lock = SPINLOCK_INIT;
    g_all_vcpus = LIST_INIT;
    g_vcpu_keys = LIST_INIT;


    // Init the user space data for the main vcpu
    g_main_vcpu.qe = LISTNODE_INIT;
    g_main_vcpu.id = (vcpuid_t)_syscall(SC_vcpu_getid);
    g_main_vcpu.groupid = 0;
    g_main_vcpu.func = NULL;
    g_main_vcpu.arg = 0;
    g_main_vcpu.specific_tab = NULL;
    g_main_vcpu.specific_capacity = 0;
    (void)_syscall(SC_vcpu_setdata, (intptr_t)&g_main_vcpu);


    List_InsertAfterLast(&g_all_vcpus, &g_main_vcpu.qe);
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

void vcpu_yield(void)
{
    (void)_syscall(SC_vcpu_yield);
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
    List_InsertAfterLast(&g_all_vcpus, &self->qe);
    spin_unlock(&g_lock);


    if ((params->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
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
    if (key) {
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

static void _vcpu_destroy_specific(vcpu_t _Nonnull self)
{
    for (int i = 0; i < self->specific_capacity; i++) {
        if (self->specific_tab[i].key) {
            vcpu_destructor_t dstr = _vcpu_key_destructor(self->specific_tab[i].key);

            if (dstr) {
                dstr(self->specific_tab[i].value);
            }
        }
    }

    free(self->specific_tab);
    self->specific_tab = NULL;
}

void *vcpu_specific(vcpu_key_t _Nonnull key)
{
    vcpu_t self = vcpu_self();

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
    }

    self->specific_tab[availSlotIdx].key = key;
    self->specific_tab[availSlotIdx].value = value;
}
