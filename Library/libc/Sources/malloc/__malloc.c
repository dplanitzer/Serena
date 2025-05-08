//
//  __malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <__stddef.h>
#include <System/_math.h>
#include <System/Mutex.h>
#include <System/Process.h>
#include "__malloc.h"

#define INITIAL_HEAP_SIZE   __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)
#define EXPANSION_HEAP_SIZE __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)


AllocatorRef        __gMainAllocator;
static os_mutex_t   __gMallocLock;


static errno_t __malloc_expand_backing_store(AllocatorRef _Nonnull pAllocator, size_t minByteCount)
{
    decl_try_err();
    const size_t ceiledSize = __Ceil_PowerOf2(minByteCount, CPU_PAGE_SIZE);
    const size_t nbytes = __min(ceiledSize, EXPANSION_HEAP_SIZE);
    char* ptr;
    
    err = os_vmalloc(nbytes, (void**)&ptr);
    if (err == EOK) {
        MemoryDescriptor md;

        md.lower = ptr;
        md.upper = ptr + nbytes;
        err = __Allocator_AddMemoryRegion(pAllocator, &md);
    }

    return err;
}

void __malloc_init(void)
{
    decl_try_err();
    MemoryDescriptor md;
    char* ptr;

    // Get backing store for our initial memory region
    err = os_vmalloc(INITIAL_HEAP_SIZE, (void**)&ptr);
    if (err != EOK) {
        abort();
    }

    md.lower = ptr;
    md.upper = md.lower + INITIAL_HEAP_SIZE;

    __gMainAllocator = __Allocator_Create(&md, __malloc_expand_backing_store);
    if (__gMainAllocator == NULL) {
        abort();
    }

    os_mutex_init(&__gMallocLock);
}

void __malloc_lock(void)
{
    os_mutex_lock(&__gMallocLock);
}

void __malloc_unlock(void)
{
    os_mutex_unlock(&__gMallocLock);
}
