//
//  __vcpu.h
//  libc
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __VCPU_H
#define __VCPU_H 1

#include <_cmndef.h>
#include <arch/_null.h>
#include <arch/_offsetof.h>
#include <ext/queue.h>
#include <sys/spinlock.h>
#include <sys/types.h>
#include <sys/vcpu.h>


typedef void (*vcpu_destructor_t)(void*);

struct vcpu_key {
    deque_node_t                qe;
    vcpu_destructor_t _Nullable destructor;
};


#define VCPU_DATA_INLINE_CAPACITY   2
#define VCPU_DATA_ENTRIES_GROW_BY   4

struct vcpu_specific {
    vcpu_key_t _Nullable    key;
    const void* _Nullable   value;
};
typedef struct vcpu_specific* vcpu_specific_t;


struct vcpu {
    deque_node_t            qe;
    vcpuid_t                id;
    vcpuid_t                groupid;
    vcpu_func_t _Nullable  func;
    void* _Nullable         arg;
    struct vcpu_specific    specific_inline[VCPU_DATA_INLINE_CAPACITY];
    vcpu_specific_t         specific_tab;
    int                     specific_capacity;
};


extern spinlock_t   __g_lock;
extern deque_t      __g_all_vcpus;
extern struct vcpu  __g_main_vcpu;
extern deque_t      __g_vcpu_keys;

// For libdispatch
extern vcpu_key_t __os_dispatch_key;


extern void __vcpu_init(void);
extern _Noreturn void __vcpu_relinquish(vcpu_t _Nonnull self);

//#define vcpu_from_wq_node(__ptr) \
//(vcpu_t*) (((uint8_t*)__ptr) - offsetof(struct vcpu, wq_node))

#endif /* __VCPU_H */
