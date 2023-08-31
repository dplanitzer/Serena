//
//  stdlib.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <__stddef.h>
#include <System.h>


void __stdlibc_init(void)
{
    __malloc_init();
}

_Noreturn abort(void)
{
    __exit(EXIT_FAILURE);
}

_Noreturn _Abort(const char* pFilename, int lineNum, const char* pFuncName)
{
    printf("%s:%d: %s: aborted\n", pFilename, lineNum, pFuncName);
    __exit(EXIT_FAILURE);
}

int atexit(void (*func)(void))
{
    // XXX implement
    return -1;
}

_Noreturn exit(int exit_code)
{
    // XXX do at_exit() stuff
    __exit(exit_code);
}

_Noreturn _Exit(int exit_code)
{
    // XXX do at_exit() stuff
    __exit(exit_code);
}
