//
//  remove.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <sys/_syscall.h>


int remove(const char* path)
{
    //XXX tell unlink to  accept a file or directory
    return (int)_syscall(SC_unlink, path);
}
