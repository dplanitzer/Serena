//
//  vcpu_specific.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <stdlib.h>


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
