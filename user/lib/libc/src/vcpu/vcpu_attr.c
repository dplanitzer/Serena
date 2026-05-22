//
//  vcpu_attr.c
//  libc
//
//  Created by Dietmar Planitzer on 5/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stddef.h>
#include <serena/vcpu_acquire.h>

int vcpu_attr_init(vcpu_attr_t* _Nonnull attr)
{
    attr->version = sizeof(vcpu_attr_t);
    attr->stack_size = 0;
    attr->group_id = VCPUID_DEFAULT_GROUP;
    attr->policy.version = sizeof(vcpu_policy_t);
    attr->policy.qos.grade = VCPU_QOS_UTILITY;
    attr->policy.qos.priority = VCPU_PRI_NORMAL;
    attr->flags = _VCPU_RESUMED;

    return 0;
}

int vcpu_attr_destroy(vcpu_attr_t* _Nullable attr)
{
    if (attr) {
        attr->version = 0;
    }

    return 0;
}


vcpuid_t vcpu_attr_groupid(const vcpu_attr_t* _Nonnull attr)
{
    return attr->group_id;
}

int vcpu_attr_setgroupid(vcpu_attr_t * _Nonnull attr, vcpuid_t id)
{
    attr->group_id = id;
    return 0;
}


bool vcpu_attr_suspended(const vcpu_attr_t* _Nonnull attr)
{
    return (attr->flags & _VCPU_RESUMED) == 0 ? true : false;
}

void vcpu_attr_setsuspended(vcpu_attr_t* _Nonnull attr, bool flag)
{
    if (flag) {
        attr->flags &= ~_VCPU_RESUMED;
    }
    else {
        attr->flags |= _VCPU_RESUMED;
    }
}

void vcpu_attr_qos(vcpu_attr_t* _Nonnull attr, int* _Nullable qos, int* _Nullable priority)
{
    if (qos) {
        *qos = attr->policy.qos.grade;
    }
    if (priority) {
        *priority = attr->policy.qos.priority;
    }
}

int vcpu_attr_setqos(vcpu_attr_t* _Nonnull attr, int qos, int priority)
{
    if (qos < VCPU_QOS_BACKGROUND || qos > VCPU_QOS_REALTIME) {
        errno = EINVAL;
        return -1;
    }
    if (priority < VCPU_PRI_LOWEST || priority > VCPU_PRI_HIGHEST) {
        errno = EINVAL;
        return -1;
    }

    attr->policy.version = sizeof(vcpu_policy_t);
    attr->policy.qos.grade = qos;
    attr->policy.qos.priority = priority;
    
    return 0;
}
