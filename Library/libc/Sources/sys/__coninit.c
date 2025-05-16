//
//  __coninit.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/_syscall.h>
#include <kern/_varargs.h>


int __coninit(void)
{
    return (int)_syscall(SC_coninit);
}
