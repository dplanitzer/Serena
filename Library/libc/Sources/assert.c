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


void _Assert(const char* _Nonnull _Restrict filename, int lineNum, const char* _Nonnull _Restrict funcName, const char* _Nonnull _Restrict expr)
{
    printf("%s:%d: Assertion '%s' failed at %s().\n", filename, lineNum, expr, funcName);
    abort();
}
