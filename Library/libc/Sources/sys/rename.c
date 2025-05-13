//
//  rename.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


int rename(const char* oldpath, const char* newpath)
{
    return (int)_syscall(SC_rename, oldpath, newpath);
}
