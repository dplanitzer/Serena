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
    SListNode   node;
    vcpuid_t    id;
    vcpuid_t    groupid;
};


extern void __vcpu_init(void);
extern void vcpu_init(vcpu_t _Nonnull self, vcpuid_t groupid);

//#define vcpu_from_wq_node(__ptr) \
//(vcpu_t*) (((uint8_t*)__ptr) - offsetof(struct vcpu, wq_node))

#endif /* __SYS_VCPU_H */
