//
//  vcpu_key.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <stdlib.h>
#include <sys/spinlock.h>
#include <kpi/syscall.h>


vcpu_key_t _Nullable vcpu_key_create(vcpu_destructor_t _Nullable destructor)
{
    vcpu_key_t key = malloc(sizeof(struct vcpu_key));

    if (key) {
        key->qe = LISTNODE_INIT;
        key->destructor = destructor;

        spin_lock(&__g_lock);
        List_InsertAfterLast(&__g_vcpu_keys, &key->qe);
        spin_unlock(&__g_lock);
    }
    return key;
}

void vcpu_key_delete(vcpu_key_t _Nullable key)
{
    if (key && key != __os_dispatch_key) {
        spin_lock(&__g_lock);
        List_Remove(&__g_vcpu_keys, &key->qe);
        spin_unlock(&__g_lock);

        free(key);
    }
}
