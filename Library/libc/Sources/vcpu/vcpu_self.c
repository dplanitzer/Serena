//
//  vcpu_self.c
//  libc
//
//  Created by Dietmar Planitzer on 6/29/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "__vcpu.h"
#include <kpi/syscall.h>


vcpu_t _Nonnull vcpu_self(void)
{
    return (vcpu_t)_syscall(SC_vcpu_getdata);
}

vcpu_t _Nonnull vcpu_main(void)
{
    return &__g_main_vcpu;
}
