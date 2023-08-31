//
//  assert.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <System.h>


_Noreturn abort(void)
{
    _Exit(EXIT_FAILURE);
}

_Noreturn _Abort(const char* pFilename, int lineNum, const char* pFuncName)
{
    printf("%s:%d: %s: aborted\n", pFilename, lineNum, pFuncName);
    _Exit(EXIT_FAILURE);
}

_Noreturn _Assert(const char* pFilename, int lineNum, const char* pFuncName, const char* expr)
{
    printf("%s:%d: %s: Assertion '%s' failed.\n", pFilename, lineNum, pFuncName, expr);
    while(1); //_Exit(EXIT_FAILURE);
}
