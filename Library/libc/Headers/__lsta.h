//
//  __lsta.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef __LSTA_H
#define __LSTA_H 1

#include <_cmndef.h>
#include <stdbool.h>
#include <stddef.h>
#define __ERRNO_T_WANTED 1
#include <kpi/_errno.h>

__CPP_BEGIN

struct lsta;
typedef struct lsta* lsta_t;

#ifndef _LSTA_MEM_DESC_DEFINED
#define _LSTA_MEM_DESC_DEFINED 1
// A memory descriptor describes a contiguous range of RAM that should be managed
// by the allocator.
typedef struct mem_desc_t {
    char* _Nonnull  lower;
    char* _Nonnull  upper;
} mem_desc_t;
#endif


// Errors passed to the mem_error function
#define MERR_CORRUPTION     1
#define MERR_DOUBLE_FREE    2


// Callback that is invoked by the allocator if it needs more backing store.
// Should return true on success and false on failure. Failure will result in a
// ENOMEM error.
typedef bool (*lsta_grow_func_t)(lsta_t _Nonnull allocator, size_t minByteCount);

// Invoked when the memory allocator has detected some kind of heap corruption
// or severe API misuse.
typedef void (*lsta_error_func_t)(int err, const char* _Nonnull funcName, void* _Nullable ptr);


extern lsta_t _Nullable __lsta_create(const mem_desc_t* _Nonnull md, lsta_grow_func_t _Nullable growFunc, lsta_error_func_t _Nonnull errFunc);

// Adds the given memory region to the allocator's available memory pool.
extern errno_t __lsta_add_memregion(lsta_t _Nonnull self, const mem_desc_t* _Nonnull md);

extern void* _Nullable __lsta_alloc(lsta_t _Nonnull self, size_t nbytes);
extern void* _Nullable __lsta_realloc(lsta_t _Nonnull self, void * _Nullable ptr, size_t new_size);

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
extern errno_t __lsta_dealloc(lsta_t _Nonnull self, void* _Nullable ptr);

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
extern errno_t __lsta_getblocksize(lsta_t _Nonnull self, void* _Nonnull ptr, size_t* _Nonnull pOutSize);

// Returns true if the given pointer is a base pointer of a memory block that
// was allocated with the given allocator.
extern bool __lsta_isvalidptr(lsta_t _Nonnull self, void* _Nullable ptr);

__CPP_END

#endif /* __LSTA_H */
