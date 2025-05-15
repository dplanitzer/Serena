//
//  unlink.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


int unlink(const char* _Nonnull path)
{
    //XXX tell unlink to  accept a file only
    return (int)_syscall(SC_unlink, path);
}
