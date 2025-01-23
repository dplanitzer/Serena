//
//  Allocator.c
//  libc
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Allocator.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <System/_math.h>


#if defined(__ILP32__)
typedef int32_t word_t;
#define WORD_SIZE       4
#define WORD_MAX    INT_MAX
// 'bhdr'
#define HEADER_PATTERN  ((word_t)0x62686472)
// 'btrl'
#define TRAILER_PATTERN  ((word_t)0x6274726c)
#elif defined(__LLP64__) || defined(__LP64__)
typedef int64_t word_t;
#define WORD_SIZE       8
#define WORD_MAX    LLONG_MAX
// 'bhdr'
#define HEADER_PATTERN  ((word_t)0x6268647272646862)
// 'btrl'
#define TRAILER_PATTERN  ((word_t)0x6274726c6c727462)
#else
#error "unknown data model"
#endif

#define MIN_GROSS_BLOCK_SIZE    (sizeof(block_header_t) + sizeof(word_t) + sizeof(block_trailer_t))
#define MAX_NET_BLOCK_SIZE      (WORD_MAX - sizeof(block_header_t) - sizeof(block_trailer_t))


#define MERR_CORRUPTION     1
#define MERR_DOUBLE_FREE    2


// A memory block (freed or allocated) has a header at the beginning (lowest address)
// and a trailer (highest address) at its end. The header and trailer store the
// block size. The size is the gross block size in terms of bytes. So it includes
// the size of the header and the trailer. The sign bit of the block size indicates
// whether the block is in allocated or freed state: sign bit set to 1 means
// allocated and sign bit set to 0 means freed.

typedef struct block_header {
    word_t  size;       // < 0 -> allocated block; > 0 -> free block; == 0 -> invalid; gross block size in bytes := |size|
    word_t  pat;        // HEADER_PATTERN
} block_header_t;

typedef struct block_trailer {
    word_t  pat;        // TRAILER_PATTERN
    word_t  size;       // < 0 -> allocated block; > 0 -> free block; == 0 -> invalid; gross block size in bytes := |size|
} block_trailer_t;


// A memory region manages a contiguous range of memory.
typedef struct mem_region {
    struct mem_region* _Nullable    next;
    char* _Nonnull                  lower;  // Lowest address from which to allocate (word aligned)
    char* _Nonnull                  upper;  // Address just beyond the last allocatable address (word aligned)
} mem_region_t;


// An allocator manages memory from a pool of memory regions.
typedef struct Allocator {
    mem_region_t* _Nonnull      first_region;
    mem_region_t* _Nonnull      last_region;
    AllocatorGrowFunc _Nullable grow_func;
} Allocator;


static void mem_error(int err, const char* _Nonnull funcName, void* _Nullable ptr);



// Initializes a new mem region structure in the given memory region. Assumes
// that the memory region structure is placed at the very bottom of the region
// and that all memory following the memory region header up to the top of the
// region should be allocatable.
// \param md the memory descriptor describing the memory region to manage
// \return pointer to the mem_region object
static mem_region_t* mem_region_create(const MemoryDescriptor* _Nonnull md)
{
    char* bptr = __Ceil_Ptr_PowerOf2(md->lower, WORD_SIZE);
    char* tptr = __Floor_Ptr_PowerOf2(md->upper, WORD_SIZE);

    if (tptr < bptr) {
        return NULL;
    }
    if ((tptr - bptr) < sizeof(mem_region_t)) {
        return NULL;
    }


    // Places the memory region header at the very bottom of the memory region
    mem_region_t* mr = (mem_region_t*)bptr;
    mr->next = NULL;
    mr->lower = __Ceil_Ptr_PowerOf2(bptr + sizeof(mem_region_t), WORD_SIZE);
    mr->upper = tptr;

    if ((mr->upper - mr->lower) < MIN_GROSS_BLOCK_SIZE) {
        return NULL;
    }


    // Cover all the rest in the memory region with a single freed block
    block_header_t* bhdr = (block_header_t*)mr->lower;
    bhdr->size = mr->upper - mr->lower;
    bhdr->pat = HEADER_PATTERN;

    block_trailer_t* btrl = (block_trailer_t*)(mr->upper - sizeof(block_trailer_t));
    btrl->size = mr->upper - mr->lower;
    btrl->pat = TRAILER_PATTERN;

    return mr;
}

static void mem_error(int err, const char* _Nonnull funcName, void* _Nullable ptr)
{
    switch (err) {
        case MERR_CORRUPTION:
            printf("** %s: heap corruption at %p\n", funcName, ptr);
            break;

        case MERR_DOUBLE_FREE:
            printf("** %s: ignoring double free at: %p\n", ptr);
            break;

        default:
            break;
    }
}

static bool __validate_block_header(block_header_t* _Nonnull bhdr, const char* _Nonnull funcName, void* _Nonnull ptr)
{
    if (bhdr->pat == HEADER_PATTERN) {
        return true;
    }

    mem_error(MERR_CORRUPTION, funcName, ptr);
    return false;
}

static bool __validate_block_trailer(block_trailer_t* _Nonnull btrl, const char* _Nonnull funcName, void* _Nonnull ptr)
{
    if (btrl->pat == TRAILER_PATTERN) {
        return true;
    }

    mem_error(MERR_CORRUPTION, funcName, ptr);
    return false;
}

// Returns true if the given memory address is managed by this memory region
// and false otherwise.
static bool mem_region_manages(const mem_region_t* _Nonnull mr, char* _Nullable addr)
{
    return (addr >= mr->lower && addr < mr->upper) ? true : false;
}

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
static size_t mem_region_block_size(void* _Nonnull ptr)
{
    char* p = ptr;
    block_header_t* bhdr = (block_header_t*)(p - sizeof(block_header_t));

    if (__validate_block_header(bhdr, "msize", p)) {
        return __abs(bhdr->size) - sizeof(block_header_t) - sizeof(block_trailer_t);
    }
    else {
        return 0;
    }
}

// Allocates 'nbytes' from the given memory region.
static void* _Nullable mem_region_alloc(mem_region_t* _Nonnull mr, size_t nbytes)
{
    if (nbytes > MAX_NET_BLOCK_SIZE) {
        return NULL;
    }

    // Find a suitable free block
    const word_t gross_nbytes = sizeof(block_header_t) + __Ceil_PowerOf2(nbytes, WORD_SIZE) + sizeof(block_trailer_t);
    char* p = mr->lower;

    while (p < mr->upper) {
        if (((block_header_t*)p)->size >= gross_nbytes) {
            break;
        }

        p += __abs(((block_header_t*)p)->size);
    }
    if (p >= mr->upper) {
        return NULL;
    }


    // We found a suitable free block. Split the front portion off for our new
    // allocated block. However, the remainder of the free block might not be
    // big enough to hold a new free block. We simply turn the whole free block
    // into the allocated block in this case.
    const word_t gross_fsize = ((block_header_t*)p)->size - gross_nbytes;

    if (gross_fsize >= MIN_GROSS_BLOCK_SIZE) {
        block_header_t* bhdr = (block_header_t*)p;
        block_trailer_t* btrl = (block_trailer_t*)(p + gross_nbytes - sizeof(block_trailer_t));
        block_header_t* fhdr = (block_header_t*)(p + gross_nbytes);
        block_trailer_t* ftrl = (block_trailer_t*)(p + ((block_header_t*)p)->size - sizeof(block_trailer_t));

        bhdr->size = -gross_nbytes;
        bhdr->pat = HEADER_PATTERN;
        btrl->size = -gross_nbytes;
        btrl->pat = TRAILER_PATTERN;

        fhdr->size = gross_fsize;
        fhdr->pat = HEADER_PATTERN;
        ftrl->size = gross_fsize;
        ftrl->pat = TRAILER_PATTERN;

        return ((char*)bhdr) + sizeof(block_header_t);
    }
    else {
        // Turn the whole free block into our allocated block
        block_header_t* bhdr = (block_header_t*)p;
        block_trailer_t* btrl = (block_trailer_t*)(p + bhdr->size - sizeof(block_trailer_t));

        bhdr->size = -bhdr->size;
        bhdr->pat = HEADER_PATTERN;
        btrl->size = -btrl->size;
        btrl->pat = TRAILER_PATTERN;

        return ((char*)bhdr) + sizeof(block_header_t);
    }
}

// Deallocates the given memory block. Expects that the memory block is managed
// by the given mem region.
// \param mr the memory region header
// \param ptr pointer to the block of the memory to free
static bool mem_region_free(mem_region_t* _Nonnull mr, void* _Nonnull ptr)
{
    char* p = ptr;

    // Calculate the 'ptr' block header, trailer & gross block size
    block_header_t* bhdr = (block_header_t*)(p - sizeof(block_header_t));
    if (!__validate_block_header(bhdr, "mfree", ptr)) {
        return false;
    }

    if (bhdr->size >= 0) {
        mem_error(MERR_DOUBLE_FREE, "mfree", p);
        return false;
    }
    
    const word_t gross_bsize = __abs(bhdr->size);
    block_trailer_t* btrl = (block_trailer_t*)((char*)bhdr + gross_bsize - sizeof(block_trailer_t));
    if (!__validate_block_trailer(btrl, "mfree", ptr)) {
        return false;
    }


    // Calculate the block header, trailer & gross block size of the block in
    // front of 'ptr'. This one may be freed or allocated.
    block_trailer_t* pred_trl = (block_trailer_t*)((char*)bhdr - sizeof(block_trailer_t));
    if (!__validate_block_trailer(pred_trl, "mfree", ptr)) {
        return false;
    }

    const word_t gross_pred_size = __abs(pred_trl->size);
    block_header_t* pred_hdr = (block_header_t*)((char*)pred_trl - gross_pred_size + sizeof(block_trailer_t));
    if (!__validate_block_header(pred_hdr, "mfree", ptr)) {
        return false;
    }


    // Calculate the block header, trailer & gross block size of the block
    // following the 'ptr' block. This one may be freed or allocated.
    block_header_t* succ_hdr = (block_header_t*)((char*)btrl + sizeof(block_trailer_t));
    if (!__validate_block_header(succ_hdr, "mfree", ptr)) {
        return false;
    }

    const word_t gross_succ_size = __abs(succ_hdr->size);
    block_trailer_t* succ_trl = (block_trailer_t*)((char*)succ_hdr + gross_succ_size - sizeof(block_trailer_t));
    if (!__validate_block_trailer(succ_trl, "mfree", ptr)) {
        return false;
    }


    // Figure out the memory configuration:
    // 0 -> pred & succ are allocated
    // 1 -> pred allocated; succ freed
    // 2 -> pred freed; succ allocated
    // 3 -> pred & succ freed
    unsigned int config = 0;
    if (succ_hdr->size >= 0) {
        config |= 0x01;
    }
    if (pred_hdr->size >= 0) {
        config |= 0x10;
    }

    switch (config) {
        case 0x00:
            // Pred & succ are allocated. Just mark the block as free
            bhdr->size = -bhdr->size;
            btrl->size = -btrl->size;
            break;

        case 0x01:
            bhdr->size = ((char*)succ_trl) - ((char*)bhdr) + sizeof(block_trailer_t);
            btrl->pat = 0;
            succ_hdr->pat = 0;
            succ_trl->size = bhdr->size;
            break;

        case 0x10:
            pred_hdr->size = ((char*)btrl) - ((char*)pred_hdr) + sizeof(block_trailer_t);
            pred_trl->pat = 0;
            bhdr->pat = 0;
            btrl->size = pred_hdr->size;
            break;

        case 0x11:
            pred_hdr->size = ((char*)succ_trl) - ((char*)pred_hdr) + sizeof(block_trailer_t);
            pred_trl->pat = 0;
            bhdr->pat = 0;
            btrl->pat = 0;
            succ_hdr->pat = 0;
            succ_trl->size = pred_hdr->size;
            break;
    }

    return true;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Allocator
// MARK: -
////////////////////////////////////////////////////////////////////////////////

// Allocates a new heap.
AllocatorRef _Nullable __Allocator_Create(const MemoryDescriptor* _Nonnull md, AllocatorGrowFunc _Nullable growFunc)
{
    AllocatorRef self = NULL;
    mem_region_t* mr = mem_region_create(md);

    if (mr) {
        self = mem_region_alloc(mr, sizeof(Allocator));
        if (self) {
            self->first_region = mr;
            self->last_region = mr;
            self->grow_func = growFunc;
        }
    }

    return self;
}

// Returns the MemRegion managing the given address. NULL is returned if this
// allocator does not manage the given address.
static mem_region_t* _Nullable __Allocator_GetMemRegionFor(AllocatorRef _Nonnull self, void* _Nullable addr)
{
    mem_region_t* mr = self->first_region;

    while (mr) {
        if (mem_region_manages(mr, addr)) {
            return mr;
        }

        mr = mr->next;
    }

    return NULL;
}

bool __Allocator_IsManaging(AllocatorRef _Nonnull self, void* _Nullable ptr)
{
    if (ptr == NULL || ((uintptr_t) ptr) == UINTPTR_MAX) {
        // Any allocator can take responsibility of that since deallocating these
        // things is a NOP anyway
        return true;
    }

    return __Allocator_GetMemRegionFor(self, ptr) != NULL;
}

// Adds the given memory region to the allocator's available memory pool.
errno_t __Allocator_AddMemoryRegion(AllocatorRef _Nonnull self, const MemoryDescriptor* _Nonnull md)
{
    decl_try_err();

    if (md->lower == NULL || md->upper == md->lower) {
        return EINVAL;
    }

    mem_region_t* mr = mem_region_create(md);
    if (mr) {
        self->last_region->next = mr;
        self->last_region = mr;
        return EOK;
    }
    return ENOMEM;
}

static errno_t __Allocator_TryExpandBackingStore(AllocatorRef _Nonnull self, size_t minByteCount)
{
    return (self->grow_func) ? self->grow_func(self, minByteCount) : ENOMEM;
}

void* _Nullable __Allocator_Allocate(AllocatorRef _Nonnull self, size_t nbytes)
{
    decl_try_err();

    // Return the "empty memory block singleton" if the requested size is 0
    if (nbytes == 0) {
        return (void*)UINTPTR_MAX;
    }
    
    
    // Go through the available memory region trying to allocate the memory block
    // until it works.
    void* ptr = NULL;
    mem_region_t* mr = self->first_region;

    while (mr) {
        ptr = mem_region_alloc(mr, nbytes);
        if (ptr != NULL) {
            break;
        }

        mr = mr->next;
    }


    // Try expanding the backing store if we've exhausted our existing memory regions
    if (ptr == NULL) {
        if ((err = __Allocator_TryExpandBackingStore(self, nbytes)) == EOK) {
            ptr = mem_region_alloc(self->last_region, nbytes);
        }
    }

    return ptr;
}

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
errno_t __Allocator_Deallocate(AllocatorRef _Nonnull self, void* _Nullable ptr)
{
    if (ptr == NULL || ((uintptr_t) ptr) == UINTPTR_MAX) {
        return EOK;
    }
    
    // Find out which memory region contains the block that we want to free
    mem_region_t* mr = __Allocator_GetMemRegionFor(self, ptr);
    if (mr == NULL) {
        // 'ptr' isn't managed by this allocator
        return ENOTBLK;
    }


    // Tell the memory region to free the memory block
    mem_region_free(mr, ptr);
    
    return EOK;
}

void* _Nullable __Allocator_Reallocate(AllocatorRef _Nonnull self, void *ptr, size_t new_size)
{
    void* np;

    const size_t old_size = (ptr) ? mem_region_block_size(ptr) : 0;
    
    if (old_size != new_size) {
        np = __Allocator_Allocate(self, new_size);

        memcpy(np, ptr, __min(old_size, new_size));
        __Allocator_Deallocate(self, ptr);
    }
    else {
        np = ptr;
    }

    return np;
}

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
errno_t __Allocator_GetBlockSize(AllocatorRef _Nonnull self, void* _Nonnull ptr, size_t* _Nonnull pOutSize)
{
    // Make sure that we actually manage this memory block
    if (__Allocator_GetMemRegionFor(self, ptr) == NULL) {
        // 'ptr' isn't managed by this allocator
        return ENOTBLK;
    }

    *pOutSize = mem_region_block_size(ptr);

    return EOK;
}
