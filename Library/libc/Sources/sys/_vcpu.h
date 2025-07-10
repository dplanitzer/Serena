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


struct vcpu {
    ListNode                node;
    vcpuid_t                id;
    vcpuid_t                groupid;
    vcpu_start_t _Nullable  func;
    void* _Nullable         arg;
};


extern void __vcpu_init(void);

//#define vcpu_from_wq_node(__ptr) \
//(vcpu_t*) (((uint8_t*)__ptr) - offsetof(struct vcpu, wq_node))

#endif /* __SYS_VCPU_H */
