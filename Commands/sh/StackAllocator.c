//
//  StackAllocator.c
//  sh
//
//  Created by Dietmar Planitzer on 1/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "StackAllocator.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <abi/_math.h>

#if __LP64__
#define HEAP_ALIGNMENT  16
#elif __ILP32__
#define HEAP_ALIGNMENT  8
#else
#error "don't know how to align heap blocks"
#endif

static errno_t StackAllocator_AllocateBackingStore(StackAllocatorRef _Nonnull self, size_t nbytes);


errno_t StackAllocator_Create(size_t pageSize, size_t pageCacheCapacity, StackAllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    StackAllocatorRef self = (StackAllocatorRef)calloc(1, sizeof(StackAllocator));

    if (self) {
        self->pageSize = (pageSize < 4*HEAP_ALIGNMENT) ? 4*HEAP_ALIGNMENT : pageSize;
        self->pageCacheBytesCapacity = pageCacheCapacity;

        *pOutAllocator = self;
        return 0;
    }

    *pOutAllocator = NULL;
    return ENOMEM;
}

void StackAllocator_Destroy(StackAllocatorRef _Nullable self)
{
    if (self) {
        self->pageCacheBytesCapacity = 0;
        StackAllocator_DeallocAll(self);

        while(self->cache) {
            struct StackPage* pCurPage = self->cache;

            self->cache = pCurPage->next;
            free(pCurPage);
        }
        self->pageCacheBytesCount = 0;

        free(self);
    }
}

void* _Nullable StackAllocator_Alloc(StackAllocatorRef _Nonnull self, size_t nbytes)
{
    if (nbytes == 0) {
        return (void*)((uintptr_t*)UINTPTR_MAX);
    }

    const size_t alignedByteCount = __Ceil_PowerOf2(nbytes, HEAP_ALIGNMENT);

    if (self->tosPtr + alignedByteCount > self->tosPageEndPtr) {
        if (StackAllocator_AllocateBackingStore(self, nbytes) != 0) {
            return NULL;
        }
    }

    char* pBasePtr = (char*)self->tosPtr;
    self->tosPtr += alignedByteCount;

    return pBasePtr;
}

void* _Nullable StackAllocator_ClearAlloc(StackAllocatorRef _Nonnull self, size_t nbytes)
{
    void* ptr = StackAllocator_Alloc(self, nbytes);

    if (ptr) {
        memset(ptr, 0, nbytes);
    }
    return ptr;
}

void StackAllocator_DeallocAll(StackAllocatorRef _Nonnull self)
{
    self->tos = NULL;
    self->tosPtr = NULL;
    self->tosPageEndPtr = NULL;

    while (self->bos) {
        struct StackPage* pCurPage = self->bos;

        self->bos = pCurPage->next;
        if (self->pageCacheBytesCount + pCurPage->pageCapacity < self->pageCacheBytesCapacity) {
            // Add the page to the page cache
            pCurPage->next = self->cache;
            self->cache = pCurPage;
            self->pageCacheBytesCount += pCurPage->pageCapacity;
        }
        else {
            free(pCurPage);
        }
    }
}

static errno_t StackAllocator_AllocateBackingStore(StackAllocatorRef _Nonnull self, size_t nbytes)
{
    assert(sizeof(struct StackPage) <= HEAP_ALIGNMENT);

    // Check our page cache to see whether we got a page that is big enough for
    // this memory request.
    struct StackPage* pPrevPage = NULL;
    struct StackPage* pCurPage = self->cache;
    while (pCurPage) {
        if (pCurPage->pageCapacity >= nbytes) {
            break;
        }

        pPrevPage = pCurPage;
        pCurPage = pCurPage->next;
    }

    if (pCurPage) {
        // We found a suitable page. Remove it from the cache
        if (pPrevPage) {
            pPrevPage->next = pCurPage->next;
        } else {
            self->cache = pCurPage->next;
        }
        self->pageCacheBytesCount -= pCurPage->pageCapacity;
    }
    else {
        // No suitable cached page available. Allocate a new one
        const size_t rawPageSize = (nbytes > self->pageSize) ? nbytes : self->pageSize;
        const alignedPageSize = __Ceil_PowerOf2(rawPageSize, HEAP_ALIGNMENT);
        const pageBlockSize = alignedPageSize + HEAP_ALIGNMENT; // 1x HEAP_ALIGNMENT for the page header portion (to ensure that the base pointer will be properly aligned)

        pCurPage = (struct StackPage*)malloc(pageBlockSize);
        if (pCurPage == NULL) {
            return ENOMEM;
        }

        pCurPage->next = NULL;
        pCurPage->pageCapacity = alignedPageSize;
    }


    // Add the new page to the top of the page stack
    if (self->tos) {
        (self->tos)->next = pCurPage;
    } else {
        self->tos = pCurPage;
        self->bos = pCurPage;
    }
    pCurPage->next = NULL;

    self->tosPtr = ((char*)pCurPage) + HEAP_ALIGNMENT;
    self->tosPageEndPtr = ((char*)pCurPage) + self->pageSize;

    return 0;
}
