//
//  globals.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ___GLOBALS_H
#define ___GLOBALS_H 1

#include <stdlib.h>
#include <abi/_kbidef.h>
#include "Allocator.h"
#include "List.h"


// Private globals
extern AllocatorRef __gAllocator;
extern struct __process_arguments_t* __gProcessArguments;
extern SList __gAtExitQueue;

#endif /* ___GLOBALS_H */
