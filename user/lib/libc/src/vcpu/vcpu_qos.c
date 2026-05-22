//
//  vcpu_qos.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <kpi/syscall.h>


int vcpu_qos(vcpu_t _Nullable vcpu, int* _Nullable qos, int* _Nullable priority)
{
    vcpu_policy_t policy;

    const int r = (int)_syscall(SC_vcpu_policy, (vcpu) ? vcpu->id : VCPUID_SELF, sizeof(vcpu_policy_t), &policy);
    if (r == 0) {
        if (qos) {
            *qos = policy.qos.grade;
        }
        if (priority) {
            *priority = policy.qos.priority;
        }
    }

    return r;
}

int vcpu_setqos(vcpu_t _Nullable vcpu, int qos, int priority)
{
    vcpu_policy_t policy;

    policy.version = sizeof(vcpu_policy_t);
    policy.qos.grade = qos;
    policy.qos.priority = priority;

    return (int)_syscall(SC_vcpu_setpolicy, (vcpu) ? vcpu->id : VCPUID_SELF, &policy);
}
