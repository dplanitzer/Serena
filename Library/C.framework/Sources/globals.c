//
//  globals.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <__globals.h>


// User visible globals
char ** environ;


// Private globals
AllocatorRef __gAllocator;
struct __process_arguments_t* __gProcessArguments;
SList __gAtExitQueue;
