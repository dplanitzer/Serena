//
//  Allocator.c
//  libc
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Allocator.h"
#include <System/Process.h>
#include <stdio.h>
#include <string.h>



////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Memory Region
// MARK: -
////////////////////////////////////////////////////////////////////////////////

// Initializes a new mem region structure in the given memory region. Assumes
// that the memory region structure is placed at the very bottom of the region
// and that all memory following the memory region header up to the top of the
// region should be allocatable.
// \param pMemRegionHeader where the mem region header structure should be placed.
//                         Usually equal to pMemDesc->lower, except for region #0
//                         which stores the allocator structure in front of it
// \param pMemDesc the memory descriptor describing the memory region to manage
// \return an error or EOK
static MemRegion* MemRegion_Create(char* _Nonnull pMemRegionHeader, const MemoryDescriptor* _Nonnull pMemDesc)
{
    char* pMemRegionBase = __Ceil_Ptr_PowerOf2(pMemRegionHeader, HEAP_ALIGNMENT);
    char* pFreeLower = __Ceil_Ptr_PowerOf2(pMemRegionBase + sizeof(MemRegion), HEAP_ALIGNMENT);
    char* pFreeUpper = __Floor_Ptr_PowerOf2(pMemDesc->upper, HEAP_ALIGNMENT);

    // Make sure that the MemRegion and the first free MemBlock fit in the memory
    // area
    if (pFreeUpper > pMemDesc->upper) {
        return NULL;
    }
    if (pFreeUpper - pFreeLower <= sizeof(MemBlock)) {
        return NULL;
    }


    // Create a single free mem block that covers the whole remaining memory
    // region (everything minus the MemRegion structure).
    MemBlock* pFreeBlock = (MemBlock*)pFreeLower;
    pFreeBlock->next = NULL;
    pFreeBlock->size = (uintptr_t)pFreeUpper - (uintptr_t)pFreeLower;


    // Create the MemRegion header
    MemRegion* pMemRegion = (MemRegion*)pMemRegionBase;
    pMemRegion->next = NULL;
    pMemRegion->lower = pMemDesc->lower;
    pMemRegion->upper = pMemDesc->upper;
    pMemRegion->first_free_block = pFreeBlock;

    return pMemRegion;
}

// Returns true if the given memory address is managed by this memory region
// and false otherwise.
static bool MemRegion_IsManaging(const MemRegion* _Nonnull pMemRegion, char* _Nullable pAddress)
{
    return (pAddress >= pMemRegion->lower && pAddress < pMemRegion->upper) ? true : false;
}

// Allocates 'nBytesToAlloc' from the given memory region. Note that
// 'nBytesToAlloc' has to include the heap block header and the correct alignment.
static MemBlock* _Nullable MemRegion_AllocMemBlock(MemRegion* _Nonnull pMemRegion, size_t nBytesToAlloc)
{
    // first fit search
    MemBlock* pCurBlock = pMemRegion->first_free_block;
    MemBlock* pFoundBlock = NULL;
    MemBlock* pPrevFoundBlock = NULL;
    
    while (pCurBlock) {
        if (pCurBlock->size >= nBytesToAlloc) {
            pFoundBlock = pCurBlock;
            break;
        }
        
        pPrevFoundBlock = pCurBlock;
        pCurBlock = pCurBlock->next;
    }
    
    if (pFoundBlock == NULL) {
        return NULL;
    }
    
    MemBlock* pAllocatedBlock = NULL;

    if (pFoundBlock->size == nBytesToAlloc) {
        // Case 1: We want to allocate the whole free block
        if (pPrevFoundBlock) {
            pPrevFoundBlock->next = pFoundBlock->next;
        }
        else {
            pMemRegion->first_free_block = pFoundBlock->next;
        }
        // Size doesn't change
        pFoundBlock->next = NULL;
        pAllocatedBlock = pFoundBlock;
    }
    else {
        // Case 2: We want to allocate the first 'nBytesToAlloc' bytes of the free block
        MemBlock* pRemainingFreeBlock = (MemBlock*)((char*)pCurBlock + nBytesToAlloc);

        pRemainingFreeBlock->next = pFoundBlock->next;
        pRemainingFreeBlock->size = pFoundBlock->size - nBytesToAlloc;

        if (pPrevFoundBlock) {
            pPrevFoundBlock->next = pRemainingFreeBlock;
        }
        else {
            pMemRegion->first_free_block = pRemainingFreeBlock;
        }

        pFoundBlock->size = nBytesToAlloc;
        pFoundBlock->next = NULL;
        pAllocatedBlock = pFoundBlock;
    }
    
    
    // Return the allocated memory
    return pAllocatedBlock;
}

// Deallocates the given memory block. Expects that the memory block is managed
// by the given mem region. Expects that the memory block is already removed from
// the allocators list of allocated memory blocks.
// \param pMemRegion the memory region header
// \param pBlockToFree pointer to the header of the memory block to free
void MemRegion_FreeMemBlock(MemRegion* _Nonnull pMemRegion, MemBlock* _Nonnull pBlockToFree)
{
    //assert(pBlockToFree->next == NULL);
    
    
    // Compute the lower and the upper bound of the block that we want to free.
    char* pLowerToFree = (char*)pBlockToFree;
    char* pUpperToFree = pLowerToFree + pBlockToFree->size;
    
    
    // Find the free memory blocks just below and above 'pBlockToFree'. These
    // will be the predecessor and successor of 'pBlockToFree' on the free list
    bool isUpperFreeBlockAdjacent = false;
    bool isLowerFreeBlockAdjacent = false;
    MemBlock* pLowerFreeBlock = NULL;
    MemBlock* pUpperFreeBlock = NULL;
    MemBlock* pCurBlock = pMemRegion->first_free_block;
    
    while (pCurBlock) {
        char* pCurBlockLower = (char*)pCurBlock;
        char* pCurBlockUpper = pCurBlockLower + pCurBlock->size;

        if (pCurBlockUpper <= pLowerToFree) {
            pLowerFreeBlock = pCurBlock;
            isLowerFreeBlockAdjacent = pCurBlockUpper == pLowerToFree;
        }
        else if (pCurBlockLower >= pUpperToFree) {
            pUpperFreeBlock = pCurBlock;
            isUpperFreeBlockAdjacent = pCurBlockLower == pUpperToFree;
            break;
        }

        pCurBlock = pCurBlock->next;
    }


    // Update the free list
    if (!isLowerFreeBlockAdjacent && !isUpperFreeBlockAdjacent) {
        // Case 1: our block is surrounded by allocated blocks.
        // Merging: nothing.
        if (pLowerFreeBlock) {
            pBlockToFree->next = pLowerFreeBlock->next;
            pLowerFreeBlock->next = pBlockToFree;
        }
        else {
            pBlockToFree->next = pUpperFreeBlock;
            pMemRegion->first_free_block = pBlockToFree;
        }
    }
    else if (isUpperFreeBlockAdjacent && !isLowerFreeBlockAdjacent) {
        // Case 2: our block has a free upper neighbor and an allocated lower neighbor
        // Merging: upper & me
        pBlockToFree->size += pUpperFreeBlock->size;

        if (pLowerFreeBlock) {
            pBlockToFree->next = pUpperFreeBlock->next;
            pLowerFreeBlock->next = pBlockToFree;
        }
        else {
            pBlockToFree->next = pUpperFreeBlock->next;
            pMemRegion->first_free_block = pBlockToFree;
        }

        pUpperFreeBlock->next = NULL;
        pUpperFreeBlock->size = 0;
    }
    else if (!isUpperFreeBlockAdjacent && isLowerFreeBlockAdjacent) {
        // Case 3: our block has a free lower neighbor and an allocated upper neighbor
        // Merging: lower & me
        pLowerFreeBlock->size += pBlockToFree->size;

        pBlockToFree->next = NULL;
        pBlockToFree->size = 0;
    }
    else {
        // Case 4: our block is surrounded by free blocks
        // Merging: upper & lower & me
        pLowerFreeBlock->size = pLowerFreeBlock->size + pBlockToFree->size + pUpperFreeBlock->size;
        pLowerFreeBlock->next = pUpperFreeBlock->next;

        pBlockToFree->next = NULL;
        pBlockToFree->size = 0;
        pUpperFreeBlock->next = NULL;
        pUpperFreeBlock->size = 0;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Allocator
// MARK: -
////////////////////////////////////////////////////////////////////////////////

// Allocates a new heap.
AllocatorRef _Nullable __Allocator_Create(const MemoryDescriptor* _Nonnull md, AllocatorGrowFunc _Nullable growFunc)
{
    AllocatorRef pAllocator = NULL;

    // Reserve space for the allocator structure in the first memory region.
    char* pAllocatorBase = __Ceil_Ptr_PowerOf2(md->lower, HEAP_ALIGNMENT);
    char* pFirstMemRegionBase = pAllocatorBase + sizeof(Allocator);

    if (pFirstMemRegionBase > md->upper) {
        return NULL;
    }

    pAllocator = (AllocatorRef)pAllocatorBase;
    pAllocator->first_region = NULL;
    pAllocator->last_region = NULL;
    pAllocator->first_allocated_block = NULL;
    
    MemRegion* pFirstRegion = MemRegion_Create(pFirstMemRegionBase, md);
    if (pFirstRegion == NULL) {
        return NULL;
    }

    pAllocator->first_region = pFirstRegion;
    pAllocator->last_region = pFirstRegion;
    pAllocator->grow_func = growFunc;
    
    return pAllocator;
}

// Returns the MemRegion managing the given address. NULL is returned if this
// allocator does not manage the given address.
static MemRegion* _Nullable __Allocator_GetMemRegionFor(AllocatorRef _Nonnull pAllocator, void* _Nullable pAddress)
{
    MemRegion* pCurRegion = pAllocator->first_region;

    while (pCurRegion) {
        if (MemRegion_IsManaging(pCurRegion, pAddress)) {
            return pCurRegion;
        }

        pCurRegion = pCurRegion->next;
    }

    return NULL;
}

bool __Allocator_IsManaging(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr)
{
    if (ptr == NULL || ((uintptr_t) ptr) == UINTPTR_MAX) {
        // Any allocator can take responsibility of that since deallocating these
        // things is a NOP anyway
        return true;
    }

    return __Allocator_GetMemRegionFor(pAllocator, ptr) != NULL;
}

// Adds the given memory region to the allocator's available memory pool.
errno_t __Allocator_AddMemoryRegion(AllocatorRef _Nonnull pAllocator, const MemoryDescriptor* _Nonnull md)
{
    decl_try_err();

    if (md->lower == NULL || md->upper == md->lower) {
        return EINVAL;
    }

    MemRegion* pMemRegion = MemRegion_Create(md->lower, md);
    if (pMemRegion) {
        pAllocator->last_region->next = pMemRegion;
        pAllocator->last_region = pMemRegion;
        pMemRegion->next = NULL;
        return EOK;
    }
    return ENOMEM;
}

static errno_t __Allocator_TryExpandBackingStore(AllocatorRef _Nonnull pAllocator, size_t minByteCount)
{
    decl_try_err();

    if (pAllocator->grow_func) {
        err = pAllocator->grow_func(pAllocator, minByteCount);
    }

    return err;
}

void* _Nullable __Allocator_Allocate(AllocatorRef _Nonnull pAllocator, size_t nbytes)
{
    decl_try_err();

    // Return the "empty memory block singleton" if the requested size is 0
    if (nbytes == 0) {
        return (void*)UINTPTR_MAX;
    }
    
    
    // Compute how many bytes we have to take from free memory
    const size_t nBytesToAlloc = __Ceil_PowerOf2(sizeof(MemBlock) + nbytes, HEAP_ALIGNMENT);
    
    
    // Go through the available memory region trying to allocate the memory block
    // until it works.
    MemBlock* pMemBlock = NULL;
    MemRegion* pCurRegion = pAllocator->first_region;
    while (pCurRegion) {
        pMemBlock = MemRegion_AllocMemBlock(pCurRegion, nBytesToAlloc);
        if (pMemBlock != NULL) {
            break;
        }

        pCurRegion = pCurRegion->next;
    }


    // Try expanding the backing store if we've exhausted our existing memory regions
    if (pMemBlock == NULL) {
        try(__Allocator_TryExpandBackingStore(pAllocator, nBytesToAlloc));
        try_null(pMemBlock, MemRegion_AllocMemBlock(pAllocator->last_region, nBytesToAlloc), ENOMEM);
    }


    // Add the allocated block to the list of allocated blocks
    pMemBlock->next = pAllocator->first_allocated_block;
    pAllocator->first_allocated_block = pMemBlock;

    // Calculate and return the user memory block pointer
    return (char*)pMemBlock + sizeof(MemBlock);

catch:
    return NULL;
}

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
errno_t __Allocator_Deallocate(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr)
{
    if (ptr == NULL || ((uintptr_t) ptr) == UINTPTR_MAX) {
        return EOK;
    }
    
    // Find out which memory region contains the block that we want to free
    MemRegion* pMemRegion = __Allocator_GetMemRegionFor(pAllocator, ptr);
    if (pMemRegion == NULL) {
        // 'ptr' isn't managed by this allocator
        return ENOTBLK;
    }
    
    MemBlock* pBlockToFree = (MemBlock*)(((char*)ptr) - sizeof(MemBlock));
    
    
    // Remove the block 'ptr' from the list of allocated blocks
    MemBlock* pPrevBlock = NULL;
    MemBlock* pCurBlock = pAllocator->first_allocated_block;
    while (pCurBlock) {
        if (pCurBlock == pBlockToFree) {
            if (pPrevBlock) {
                pPrevBlock->next = pBlockToFree->next;
            } else {
                pAllocator->first_allocated_block = pBlockToFree->next;
            }
            pBlockToFree->next = NULL;
            break;
        }
        
        pPrevBlock = pCurBlock;
        pCurBlock = pCurBlock->next;
    }
    

    // Looks like 'ptr' isn't a pointer to an allocated memory block
    if (pBlockToFree->next != NULL) {
        return ENOTBLK;
    }
    
    
    // Tell the memory region to free the memory block
    MemRegion_FreeMemBlock(pMemRegion, pBlockToFree);
    
    return EOK;
}

void* _Nullable __Allocator_Reallocate(AllocatorRef _Nonnull pAllocator, void *ptr, size_t new_size)
{
    void* np;

    const size_t old_size = (ptr) ? __Allocator_GetBlockSize(pAllocator, ptr) : 0;
    
    if (old_size != new_size) {
        np = __Allocator_Allocate(pAllocator, new_size);

        memcpy(np, ptr, __min(old_size, new_size));
        __Allocator_Deallocate(pAllocator, ptr);
    }
    else {
        np = ptr;
    }

    return np;
}

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
size_t __Allocator_GetBlockSize(AllocatorRef _Nonnull pAllocator, void* _Nonnull ptr)
{
    MemBlock* pMemBlock = (MemBlock*) (((char*)ptr) - sizeof(MemBlock));

    return pMemBlock->size - sizeof(MemBlock);
}

#if 0
void __Allocator_Dump(AllocatorRef _Nonnull pAllocator)
{
    puts("Free:");
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;
    while (pCurRegion != NULL) {
        MemBlock* pCurBlock = pCurRegion->first_free_block;
        int i = 1;
        
        printf(" Region: %p - %p, s: %p\n", pCurRegion->lower, pCurRegion->upper, pCurRegion->first_free_block);
        while (pCurBlock) {
            printf("  %d:  %p: {a: %p, n: %p, s: %zu}\n", i, ((char*)pCurBlock) + sizeof(MemBlock), pCurBlock, pCurBlock->next, pCurBlock->size);
            pCurBlock = pCurBlock->next;
            i++;
        }

        pCurRegion = (MemRegion*)pCurRegion->node.next;
    }
    putchar('\n');

    MemBlock* pCurBlock = pAllocator->first_allocated_block;
    int i = 1;
    printf("Allocated (s: %p):\n", pCurBlock);
    while (pCurBlock) {
        char* pCurBlockBase = (char*)pCurBlock;
        
        printf(" %d:  %p, {a: %p, n: %p s: %zu}\n", i, pCurBlockBase + sizeof(MemBlock), pCurBlock, pCurBlock->next, pCurBlock->size);
        pCurBlock = pCurBlock->next;
        i++;
    }    
    putchar('\n');
}

void __Allocator_DumpMemoryRegions(AllocatorRef _Nonnull pAllocator)
{
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;

    puts("Mem Regions:\n");
    while (pCurRegion != NULL) {
        printf("   [%p - %p] (%zu)\n", pCurRegion->lower, pCurRegion->upper, (uintptr_t)pCurRegion->upper - (uintptr_t)pCurRegion->lower);

        pCurRegion = (MemRegion*)pCurRegion->node.next;
    }
    putchar('\n');
}
#endif
