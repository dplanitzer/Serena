//
//  assert.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


void _Assert_failed0(void)
{
    abort();
}

void _Assert_failed1(int lineno, const char* _Nonnull _Restrict funcname)
{
    printf("%s:%d: assertion failed.\n", funcname, lineno);
    abort();
}

void _Assert_failed2(int lineno, const char* _Nonnull _Restrict funcname, const char* _Nonnull _Restrict expr)
{
    printf("%s:%d: assertion '%s' failed.\n", funcname, lineno, expr);
    abort();
}

void _Assert_failed3(const char* _Nonnull _Restrict filename, int lineno, const char* _Nonnull _Restrict funcname, const char* _Nonnull _Restrict expr)
{
    printf("%s:%s:%d: assertion '%s' failed.\n", filename, funcname, lineno, expr);
    abort();
}
