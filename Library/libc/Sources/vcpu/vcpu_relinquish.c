//
//  vcpu_relinquish.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <stdlib.h>
#include <sys/spinlock.h>
#include <kpi/syscall.h>


static vcpu_destructor_t _Nullable __vcpu_key_destructor(vcpu_key_t _Nonnull key)
{
    vcpu_destructor_t dstr = NULL;

    spin_lock(&__g_lock);
    deque_for_each(&__g_vcpu_keys, deque_node_t, {
        vcpu_key_t cep = (vcpu_key_t)pCurNode;

        if (cep == key) {
            dstr = cep->destructor;
            break; 
        }
    });

    spin_unlock(&__g_lock);

    return dstr;
}

static void __vcpu_destroy_specifics(vcpu_specific_t _Nonnull tab, int tabsiz)
{
    for (int i = 0; i < tabsiz; i++) {
        if (tab[i].key) {
            vcpu_destructor_t dstr = __vcpu_key_destructor(tab[i].key);

            if (dstr) {
                dstr(tab[i].value);
            }
        }

        tab[i].key = NULL;
        tab[i].value = NULL;
    }
}

static void __vcpu_destroy_specific(vcpu_t _Nonnull self)
{
    __vcpu_destroy_specifics(self->specific_inline, VCPU_DATA_INLINE_CAPACITY);
    
    if (self->specific_capacity > 0) {
        __vcpu_destroy_specifics(self->specific_tab, self->specific_capacity);
        free(self->specific_tab);
        self->specific_tab = NULL;
    }
}

_Noreturn __vcpu_relinquish(vcpu_t _Nonnull self)
{
    spin_lock(&__g_lock);
    deque_remove(&__g_all_vcpus, &self->qe);
    spin_unlock(&__g_lock);

    if (self != &__g_main_vcpu) {
        __vcpu_destroy_specific(self);
        free(self);
    }

    (_Noreturn)_syscall(SC_vcpu_relinquish_self);
    /* NOT REACHED */
}

_Noreturn vcpu_relinquish_self(void)
{
    __vcpu_relinquish(vcpu_self());
    /* NOT REACHED */
}
