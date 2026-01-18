//
//  __malloc.c
//  libc
//
//  Created by Dietmar Planitzer on 8/24/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <arch/cpu.h>
#include <errno.h>
#include <stdio.h>
#include <__stdlib.h>
#include <ext/math.h>
#include <sys/vm.h>
#include "__malloc.h"

#define INITIAL_HEAP_SIZE   __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)
#define EXPANSION_HEAP_SIZE __Ceil_PowerOf2(64*1024, CPU_PAGE_SIZE)


lsta_t  __gMainAllocator;
bool    __gAbortOnNoMem;
mtx_t   __gMallocLock;


void _abort_on_nomem(void)
{
    __malloc_lock();
    __gAbortOnNoMem = true;
    __malloc_unlock();
}

void __malloc_nomem(void)
{
    if (!__gAbortOnNoMem) {
        errno = ENOMEM;
        return;
    }

    puts("Out of memory");
    abort();
    /* NOT REACHED */
}


static bool __malloc_expand_backing_store(lsta_t _Nonnull pAllocator, size_t minByteCount)
{
    const size_t ceiledSize = __Ceil_PowerOf2(minByteCount, CPU_PAGE_SIZE);
    const size_t nbytes = __min(ceiledSize, EXPANSION_HEAP_SIZE);
    char* ptr;
    
    if (vm_alloc(nbytes, (void**)&ptr) == 0) {
        mem_desc_t md;

        md.lower = ptr;
        md.upper = ptr + nbytes;
        if (__lsta_add_memregion(pAllocator, &md) == EOK) {
            return true;
        }
    }

    return false;
}

static void __malloc_error(int err, const char* _Nonnull _Restrict funcName, void* _Nullable _Restrict ptr)
{
    if (err = MERR_DOUBLE_FREE) {
        fprintf(stderr, "** m%s: ignoring double free at: %p\n", funcName, ptr);
    }
    else {
        fprintf(stderr, "** m%s: heap corruption at %p\n", funcName, ptr);
    }
}


void __malloc_init(void)
{
    mem_desc_t md;
    char* ptr;

    // Get backing store for our initial memory region
    if (vm_alloc(INITIAL_HEAP_SIZE, (void**)&ptr) != 0) {
        abort();
    }

    md.lower = ptr;
    md.upper = md.lower + INITIAL_HEAP_SIZE;

    __gMainAllocator = __lsta_create(&md, __malloc_expand_backing_store, __malloc_error);
    if (__gMainAllocator == NULL) {
        abort();
    }

    mtx_init(&__gMallocLock);
}
