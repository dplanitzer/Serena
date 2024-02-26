//
//  abort.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>


_Noreturn abort(void)
{
    _Exit(EXIT_FAILURE);
}

_Noreturn _Abort(const char* _Nonnull pFilename, int lineNum, const char* _Nonnull pFuncName)
{
    printf("%s:%d: %s: aborted\n", pFilename, lineNum, pFuncName);
    _Exit(EXIT_FAILURE);
}
