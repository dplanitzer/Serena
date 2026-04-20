//
//  __env_init.c
//  libc
//
//  Created by Dietmar Planitzer on 4/20/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <__stdlib.h>

char**  environ;
size_t  __gEnvironCount;
char**  __gInitialEnviron;


void __env_init(char ** initial_envp)
{
    environ = initial_envp;
    __gInitialEnviron = initial_envp;

    // calculate envc
    char** p = environ;
    while (*p++);
    __gEnvironCount = p - environ - 1;
}
