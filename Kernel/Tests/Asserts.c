//
//  Asserts.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


void Assert(const char* _Nonnull pFuncName, int lineNum, const char* _Nonnull expr)
{
    printf("%s:%d: Assertion failed: %s.\n", pFuncName, lineNum, expr);
    while (true);
}
