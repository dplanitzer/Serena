//
//  __malloc.h
//  libc
//
//  Created by Dietmar Planitzer on 8/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef __MALLOC_H
#define __MALLOC_H 1

#include "Allocator.h"

// The allocator that represents the application heap
extern AllocatorRef __kAllocator_Main;

extern void __malloc_init(void);

#endif /* __MALLOC_H */
