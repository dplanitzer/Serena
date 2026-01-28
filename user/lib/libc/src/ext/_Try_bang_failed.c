//
//  _Try_bang_failed.c
//  libc
//
//  Created by Dietmar Planitzer on 1/6/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <_cmndef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include <ext/errno.h>


_Noreturn void _Try_bang_failed1(int lineno, const char* _Nonnull funcname, errno_t err)
{
    printf("%s:%d: try failed [%d].\n", funcname, lineno, err);
    abort();
}

_Noreturn void _Try_bang_failed2(const char* _Nonnull filename, int lineno, const char* _Nonnull funcname, errno_t err)
{
    printf("%s:%s:%d: try failed [%d].\n", filename, funcname, lineno, err);
    abort();
}

_Noreturn void _Try_bang_failed0(void)
{
    printf("try failed\n");
    abort();
}
