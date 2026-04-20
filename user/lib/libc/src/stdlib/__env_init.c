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


void __env_init(size_t envc, char ** initial_envp)
{
    environ = initial_envp;
    __gEnvironCount = envc;
    __gInitialEnviron = initial_envp;
}
