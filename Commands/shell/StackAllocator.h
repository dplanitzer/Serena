//
//  StackAllocator.h
//  sh
//
//  Created by Dietmar Planitzer on 1/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef StackAllocator_h
#define StackAllocator_h

#include <stdbool.h>
#include <System/System.h>

struct StackPage;


// XXX not yet
//typedef struct StackMarker {
//    struct StackPage* _Nonnull  page;
//} StackMarker;

struct StackPage {
    struct StackPage* _Nullable next;
    size_t                      pageCapacity;   // page capacity without its header
};

typedef struct StackAllocator {
    struct StackPage* _Nullable bos;    // bottom of stack page (page->next points to the next page which is closer to the tos)
    struct StackPage* _Nullable tos;    // top of stack page
    struct StackPage* _Nullable cache;  // first page in page cache

    volatile char* _Nullable    tosPtr;
    volatile char* _Nullable    tosPageEndPtr;

    size_t                      pageSize;               // unaligned size of a page without the page header
    size_t                      pageCacheBytesCapacity; // page cache capacity without page headers
    size_t                      pageCacheBytesCount;    // number of bytes available in page cache without page headers
} StackAllocator;
typedef StackAllocator* StackAllocatorRef;


extern errno_t StackAllocator_Create(size_t pageSize, size_t pageCacheCapacity, StackAllocatorRef _Nullable * _Nonnull pOutAllocator);
extern void StackAllocator_Destroy(StackAllocatorRef _Nullable self);

// Allocates 'nbytes' bytes from the top of the stack and guarantees that the
// returned pointer will be aligned appropriately and for access efficiency.
extern void* _Nullable StackAllocator_Alloc(StackAllocatorRef _Nonnull self, size_t nbytes);
extern void* _Nullable StackAllocator_ClearAlloc(StackAllocatorRef _Nonnull self, size_t nbytes);
extern void StackAllocator_DeallocAll(StackAllocatorRef _Nonnull self);

#endif  /* StackAllocator_h */
