//
//  globals.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__globals.h>
#include <stdio.h>


// User visible globals
char ** environ;
FILE* __Stdin;
FILE* __Stdout;
FILE* __Stderr;

// Private globals
AllocatorRef __gAllocator;
struct __process_arguments_t* __gProcessArguments;
SList __gAtExitQueue;
