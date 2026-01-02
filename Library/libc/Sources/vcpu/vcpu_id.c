//
//  vcpu_id.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"


vcpuid_t vcpu_id(vcpu_t _Nonnull self)
{
    return self->id;
}

vcpuid_t vcpu_groupid(vcpu_t _Nonnull self)
{
    return self->groupid;
}
