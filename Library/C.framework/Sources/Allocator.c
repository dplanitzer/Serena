//
//  Allocator.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Allocator.h"
#include <errno.h>
#include <List.h>
#include <stdio.h>

#if __LP64__
#define HEAP_ALIGNMENT  16
#elif __ILP32__
#define HEAP_ALIGNMENT  8
#else
#error "don't know how to align heap blocks"
#endif


// A memory block structure describes a freed or allocated block of memory. The
// structure is placed right in front of the memory block. Note that the block
// size includes the header size.
typedef struct _MemBlock {
    struct _MemBlock* _Nullable next;
    size_t                      size;   // The size includes sizeof(MemBlock). Max size of a free block is 4GB; max size of an allocated block is 2GB
} MemBlock;


// A heap memory region is a region of contiguous memory which is managed by the
// heap. Each such region has its own private list of free memory blocks.
typedef struct _MemRegion {
    SListNode           node;
    char* _Nonnull      lower;
    char* _Nonnull      upper;
    MemBlock* _Nullable first_free_block;   // Every memory region has its own private free list. Note that the blocks on this list are ordered by increasing base address
} MemRegion;


// An allocator manages memory from a pool of memory contiguous regions.
typedef struct _Allocator {
    SList               regions;
    MemBlock* _Nullable first_allocated_block;  // Unordered list of allocated blocks (no matter from which memory region they were allocated)
} Allocator;



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
    pFreeBlock->size = pFreeUpper - pFreeLower;


    // Create the MemRegion header
    MemRegion* pMemRegion = (MemRegion*)pMemRegionBase;
    SListNode_Init(&pMemRegion->node);
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
    assert(pBlockToFree->next == NULL);
    
    
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

// Allocates a new heap. An allocator manages the memory region described by the
// given memory descriptor. Additional memory regions may be added later. The
// heap management data structures are stored inside those memory regions.
// \param pMemDesc the initial memory region to manage
// \param pOutAllocator receives the allocator reference
// \return an error or EOK
errno_t __Allocator_Create(const MemoryDescriptor* _Nonnull pMemDesc, AllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    decl_try_err();

    // Reserve space for the allocator structure in the first memory region.
    char* pAllocatorBase = __Ceil_Ptr_PowerOf2(pMemDesc->lower, HEAP_ALIGNMENT);
    char* pFirstMemRegionBase = pAllocatorBase + sizeof(Allocator);

    AllocatorRef pAllocator = (AllocatorRef)pAllocatorBase;
    pAllocator->first_allocated_block = NULL;
    SList_Init(&pAllocator->regions);
    
    MemRegion* pFirstRegion;
    try_null(pFirstRegion, MemRegion_Create(pFirstMemRegionBase, pMemDesc), ENOMEM);
    SList_InsertAfterLast(&pAllocator->regions, &pFirstRegion->node);
    
    *pOutAllocator = pAllocator;
    return 0;

catch:
    *pOutAllocator = NULL;
    return err;
}

// Adds the given memory region to the allocator's available memory pool.
errno_t __Allocator_AddMemoryRegion(AllocatorRef _Nonnull pAllocator, const MemoryDescriptor* _Nonnull pMemDesc)
{
    MemRegion* pMemRegion;
    decl_try_err();

    try_null(pMemRegion, MemRegion_Create(pMemDesc->lower, pMemDesc), ENOMEM);
    SList_InsertAfterLast(&pAllocator->regions, &pMemRegion->node);
    return 0;

catch:
    return err;
}

// Returns the MemRegion managing the given address. NULL is returned if this
// allocator does not manage the given address.
static MemRegion* _Nullable Allocator_GetMemRegionManaging_Locked(AllocatorRef _Nonnull pAllocator, void* _Nullable pAddress)
{
    SList_ForEach(&pAllocator->regions, MemRegion, {
        if (MemRegion_IsManaging(pCurNode, pAddress)) {
            return pCurNode;
        }
    });

    return NULL;
}

bool __Allocator_IsManaging(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr)
{
    if (ptr == NULL || ((uintptr_t) ptr) == UINTPTR_MAX) {
        // Any allocator can take responsibility of that since deallocating these
        // things is a NOP anyway
        return true;
    }

    const bool r = Allocator_GetMemRegionManaging_Locked(pAllocator, ptr) != NULL;
    return r;
}

errno_t __Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, size_t nbytes, void* _Nullable * _Nonnull pOutPtr)
{
    // Return the "empty memory block singleton" if the requested size is 0
    if (nbytes == 0) {
        *((uintptr_t*) pOutPtr) = UINTPTR_MAX;
        return 0;
    }
    
    
    // Compute how many bytes we have to take from free memory
    const size_t nBytesToAlloc = __Ceil_PowerOf2(sizeof(MemBlock) + nbytes, HEAP_ALIGNMENT);
    
    
    // Note that the code here assumes desc 0 is chip RAM and all the others are
    // fast RAM. This is enforced by Allocator_Create().
    MemBlock* pMemBlock = NULL;
    decl_try_err();

    // Go through the available memory region trying to allocate the memory block
    // until it works.
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;
    while (pCurRegion) {
        pMemBlock = MemRegion_AllocMemBlock(pCurRegion, nBytesToAlloc);
        if (pMemBlock != NULL) {
            break;
        }

        pCurRegion = (MemRegion*)pCurRegion->node.next;
    }


    // Make sure we got the memory
    throw_ifnull(pMemBlock, ENOMEM);


    // Add the allocated block to the list of allocated blocks
    pMemBlock->next = pAllocator->first_allocated_block;
    pAllocator->first_allocated_block = pMemBlock;

    // Calculate and return the user memory block pointer
    *pOutPtr = (char*)pMemBlock + sizeof(MemBlock);
    return 0;

catch:
    *pOutPtr = NULL;
    return err;
}

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
errno_t __Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, void* _Nullable ptr)
{
    if (ptr == NULL || ((uintptr_t) ptr) == UINTPTR_MAX) {
        return 0;
    }
    
    // Find out which memory region contains the block that we want to free
    MemRegion* pMemRegion = Allocator_GetMemRegionManaging_Locked(pAllocator, ptr);
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
    
    return 0;
}

// Returns the size of the given memory block. This is the size minus the block
// header and plus whatever additional memory the allocator added based on its
// internal alignment constraints.
size_t __Allocator_GetBlockSize(AllocatorRef _Nonnull pAllocator, void* _Nonnull ptr)
{
    MemBlock* pMemBlock = (MemBlock*) (((char*)ptr) - sizeof(MemBlock));

    return pMemBlock->size - sizeof(MemBlock);
}

#ifdef ALLOCATOR_DEBUG
void __Allocator_Dump(AllocatorRef _Nonnull pAllocator)
{
    puts("Free:");
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;
    while (pCurRegion != NULL) {
        MemBlock* pCurBlock = pCurRegion->first_free_block;
        int i = 1;
        
        printf(" Region: 0x%p - 0x%p, s: 0x%p\n", pCurRegion->lower, pCurRegion->upper, pCurRegion->first_free_block);
        while (pCurBlock) {
            printf("  %d:  0x%p: {a: 0x%p, n: 0x%p, s: %zd}\n", i, ((char*)pCurBlock) + sizeof(MemBlock), pCurBlock, pCurBlock->next, pCurBlock->size);
            pCurBlock = pCurBlock->next;
            i++;
        }

        pCurRegion = (MemRegion*)pCurRegion->node.next;
    }
    puts("");

    MemBlock* pCurBlock = pAllocator->first_allocated_block;
    int i = 1;
    printf("Allocated (s: 0x%p):\n", pCurBlock);
    while (pCurBlock) {
        char* pCurBlockBase = (char*)pCurBlock;
        MemRegion* pMemRegion = Allocator_GetMemRegionManaging_Locked(pAllocator, pCurBlockBase);
        
        printf(" %d:  0x%p, {a: 0x%p, n: 0x%p s: %zd}\n", i, pCurBlockBase + sizeof(MemBlock), pCurBlock, pCurBlock->next, pCurBlock->size);
        pCurBlock = pCurBlock->next;
        i++;
    }    
    puts("");
}

void __Allocator_DumpMemoryRegions(AllocatorRef _Nonnull pAllocator)
{
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;

    puts("Mem Regions:\n");
    while (pCurRegion != NULL) {
        printf("   lower: 0x%p, upper: 0x%p\n", pCurRegion->lower, pCurRegion->upper);

        pCurRegion = (MemRegion*)pCurRegion->node.next;
    }
    puts("");
}
#endif
