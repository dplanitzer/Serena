//
//  sys/_vcpu.h
//  libc
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_VCPU_H
#define __SYS_VCPU_H 1

#include <_cmndef.h>
#include <_null.h>
#include <_offsetof.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/vcpu.h>


typedef void (*vcpu_destructor_t)(void*);

struct vcpu_key {
    ListNode                    qe;
    vcpu_destructor_t _Nullable destructor;
};


#define VCPU_DATA_ENTRIES_GROW_BY   4

struct vcpu_specific {
    vcpu_key_t _Nullable    key;
    const void* _Nullable   value;
};
typedef struct vcpu_specific* vcpu_specific_t;


struct vcpu {
    ListNode                qe;
    vcpuid_t                id;
    vcpuid_t                groupid;
    vcpu_func_t _Nullable  func;
    void* _Nullable         arg;
    struct vcpu_specific    owner_specific;
    vcpu_specific_t         specific_tab;
    int                     specific_capacity;
};


extern void __vcpu_init(void);

//#define vcpu_from_wq_node(__ptr) \
//(vcpu_t*) (((uint8_t*)__ptr) - offsetof(struct vcpu, wq_node))

#endif /* __SYS_VCPU_H */
