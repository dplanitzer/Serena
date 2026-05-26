//
//  asserts.c
//  libdispatch tests
//
//  Created by Dietmar Planitzer on 5/25/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


static void vAssert(const char* _Nonnull pFuncName, int lineNum, const char* _Nonnull fmt, va_list ap)
{
    printf("%s:%d: Assertion failed: ", pFuncName, lineNum);
    vprintf(fmt, ap);
    puts(".");
    while (true);
}

void Assert(const char* _Nonnull pFuncName, int lineNum, const char* _Nonnull fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vAssert(pFuncName, lineNum, fmt, ap);
    va_end(ap);
    while (true);
}
