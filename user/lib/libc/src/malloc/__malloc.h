//
//  __malloc.h
//  libc
//
//  Created by Dietmar Planitzer on 8/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef __MALLOC_H
#define __MALLOC_H 1

#include <__lsta.h>
#include <sys/mtx.h>


// The allocator that represents the application heap
extern lsta_t   __gMainAllocator;
extern bool     __gAbortOnNoMem;
extern mtx_t    __gMallocLock;


extern void __malloc_nomem(void);

#define __malloc_lock() \
mtx_lock(&__gMallocLock)

#define __malloc_unlock() \
mtx_unlock(&__gMallocLock)

#endif /* __MALLOC_H */
