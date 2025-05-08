//
//  globals.c
//  libc
//
//  Created by Dietmar Planitzer on 9/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__globals.h>
#include <stdio.h>


// User visible globals
char ** environ;

// Private globals
pargs_t* __gProcessArguments;
