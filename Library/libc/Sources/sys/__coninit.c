//
//  __coninit.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>


int __coninit(void)
{
    return (int)_syscall(SC_coninit);
}
