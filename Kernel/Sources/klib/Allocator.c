//
//  Allocator.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Allocator.h"
#include "Foundation.h"
#include "List.h"


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
    UInt                        size;   // Max size of a free block is 4GB; max size of an allocated block is 2GB
} MemBlock;


// A heap memory region is a region of contiguous memory which is managed by the
// heap. Each such region has its own private list of free memory blocks.
typedef struct _MemRegion {
    SListNode           node;
    Byte* _Nonnull      lower;
    Byte* _Nonnull      upper;
    MemBlock* _Nullable first_free_block;   // Every memory region has its own private free list
} MemRegion;


// An allocator manages memory from a pool of memory contiguous regions.
typedef struct _Allocator {
    SList               regions;
    MemBlock* _Nullable first_allocated_block;
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
static MemRegion* MemRegion_Create(Byte* _Nonnull pMemRegionHeader, const MemoryDescriptor* _Nonnull pMemDesc)
{
    Byte* pMemRegionBase = align_up_byte_ptr(pMemRegionHeader, HEAP_ALIGNMENT);
    Byte* pFreeLower = align_up_byte_ptr(pMemRegionBase + sizeof(MemRegion), HEAP_ALIGNMENT);
    Byte* pFreeUpper = align_down_byte_ptr(pMemDesc->upper, HEAP_ALIGNMENT);

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
static Bool MemRegion_IsManaging(const MemRegion* _Nonnull pMemRegion, Byte* _Nullable pAddress)
{
    return (pAddress >= pMemRegion->lower && pAddress < pMemRegion->upper) ? true : false;
}

// Allocates 'nBytesToAlloc' from the given memory region. Note that
// 'nBytesToAlloc' has to include the heap block header and the correct alignment.
static MemBlock* _Nullable MemRegion_AllocMemBlock(MemRegion* _Nonnull pMemRegion, Int nBytesToAlloc)
{
    // first fit search
    MemBlock* pPrevBlock = NULL;
    MemBlock* pCurBlock = pMemRegion->first_free_block;
    MemBlock* pFoundBlock = NULL;
    
    while (pCurBlock) {
        if (pCurBlock->size >= nBytesToAlloc) {
            pFoundBlock = pCurBlock;
            break;
        }
        
        pPrevBlock = pCurBlock;
        pCurBlock = pCurBlock->next;
    }
    
    if (pFoundBlock == NULL) {
        return NULL;
    }
    
    // Save the pointer to the next free block (because we may override this
    // pointer below)
    MemBlock* pNextFreeBlock = pCurBlock->next;
    
    
    // Split the existing free block into an allocated block and a new
    // (and smaller) free block
    UInt nNewFreeBytes = pCurBlock->size - nBytesToAlloc;
    MemBlock* pAllocedBlock = pCurBlock;
    MemBlock* pNewFreeBlock = (MemBlock*)((Byte*)pCurBlock + nBytesToAlloc);
    
    
    // Initialize the allocated block
    pAllocedBlock->next = NULL;
    pAllocedBlock->size = nBytesToAlloc;
    
    
    // Initialize the new free block and replace the old one in the free list
    // with this new one
    pNewFreeBlock->next = pNextFreeBlock;
    pNewFreeBlock->size = nNewFreeBytes;
    if (pPrevBlock) {
        pPrevBlock->next = pNewFreeBlock;
    } else {
        pMemRegion->first_free_block = pNewFreeBlock;
    }
    
    
    // Return the allocated memory
    return pAllocedBlock;
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
    Byte* pLowerToFree = (Byte*)pBlockToFree;
    Byte* pUpperToFree = pLowerToFree + pBlockToFree->size;
    
    
    // Go through the free list and find the block that is right below the block
    // we want to free and the block that is right above the block we want to
    // free. We'll then merge everything into the lowest block and we remove the
    // highest block from the free list. That's the simplest way to do things.
    // NOTE: an allocated block may be bordered by a free block on both sides!
    MemBlock* pUpperPrevFreeBlock = NULL;
    MemBlock* pLowerPrevFreeBlock = NULL;
    MemBlock* pUpperFreeBlock = NULL;
    MemBlock* pLowerFreeBlock = NULL;
    MemBlock* pPrevBlock = NULL;
    MemBlock* pCurBlock = pMemRegion->first_free_block;

    while (pCurBlock) {
        Byte* pCurBlockLower = (Byte*)pCurBlock;
        Byte* pCurBlockUpper = pCurBlockLower + pCurBlock->size;
        
        if (pCurBlockLower == pUpperToFree) {
            // This is the block above the block we want to free
            pUpperFreeBlock = pCurBlock;
            pUpperPrevFreeBlock = pPrevBlock;
        }
        
        if (pCurBlockUpper == pLowerToFree) {
            // This is the block below the block we want to free
            pLowerFreeBlock = pCurBlock;
            pLowerPrevFreeBlock = pPrevBlock;
        }
        
        if (pUpperFreeBlock && pLowerFreeBlock) {
            break;
        }
        
        pPrevBlock = pCurBlock;
        pCurBlock = pCurBlock->next;
    }
    
    if (pLowerFreeBlock) {
        // Case 1: adjacent to lower free block -> merge everything into the
        // lower free block
        pLowerFreeBlock->size += pBlockToFree->size;
        pLowerFreeBlock->size += pUpperFreeBlock->size;
        
        if (pUpperPrevFreeBlock) {
            pUpperPrevFreeBlock->next = pUpperFreeBlock->next;
        } else {
            pMemRegion->first_free_block = pUpperFreeBlock->next;
        }
        
        pUpperFreeBlock->next = NULL;
        pUpperFreeBlock->size = 0;
        pBlockToFree->next = NULL;
        pBlockToFree->size = 0;
    }
    else if (pUpperFreeBlock) {
        // Case 2: adjacent to upper free block -> merge everything into the
        // block we want to free
        pBlockToFree->size += pUpperFreeBlock->size;
        
        pBlockToFree->next = pUpperFreeBlock->next;
        if (pUpperPrevFreeBlock) {
            pUpperPrevFreeBlock->next = pBlockToFree;
        } else {
            pMemRegion->first_free_block = pBlockToFree;
        }
        
        pUpperFreeBlock->next = NULL;
        pUpperFreeBlock->size = 0;
    }
    else {
        // Case 3: no adjacent free block -> add the block-to-free as is to the
        // free list
        pBlockToFree->next = pMemRegion->first_free_block;
        pMemRegion->first_free_block = pBlockToFree;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Allocator
// MARK: -
////////////////////////////////////////////////////////////////////////////////

// Allocates a new heap. An allocator manages the memory region described by the
// given memory descriptor. Additional memory regions may be added later. The
// heap managment data structures are stored inside those memory regions.
// \param pMemDesc the initial memory region to manage
// \param pOutAllocator receives the allocator reference
// \return an error or EOK
ErrorCode Allocator_Create(const MemoryDescriptor* _Nonnull pMemDesc, AllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    decl_try_err();

    // Reserve space for the allocator structure in the first memory region.
    Byte* pAllocatorBase = align_up_byte_ptr(pMemDesc->lower, HEAP_ALIGNMENT);
    Byte* pFirstMemRegionBase = pAllocatorBase + sizeof(Allocator);

    AllocatorRef pAllocator = (AllocatorRef)pAllocatorBase;
    pAllocator->first_allocated_block = NULL;
    SList_Init(&pAllocator->regions);
    
    MemRegion* pFirstRegion;
    try_null(pFirstRegion, MemRegion_Create(pFirstMemRegionBase, pMemDesc), ENOMEM);
    SList_InsertAfterLast(&pAllocator->regions, &pFirstRegion->node);
    
    *pOutAllocator = pAllocator;
    return EOK;

catch:
    *pOutAllocator = NULL;
    return err;
}

// Adds the given memory region to the allocator's available memory pool.
ErrorCode Allocator_AddMemoryRegion(AllocatorRef _Nonnull pAllocator, const MemoryDescriptor* _Nonnull pMemDesc)
{
    MemRegion* pMemRegion;
    decl_try_err();

    try_null(pMemRegion, MemRegion_Create(pMemDesc->lower, pMemDesc), ENOMEM);
    SList_InsertAfterLast(&pAllocator->regions, &pMemRegion->node);
    return EOK;

catch:
    return err;
}

// Returns the MemRegion managing the given address. NULL is returned if this
// allocator does not manage the given address.
static MemRegion* _Nullable Allocator_GetMemRegionManaging_Locked(AllocatorRef _Nonnull pAllocator, Byte* _Nullable pAddress)
{
    SList_ForEach(&pAllocator->regions, MemRegion, {
        if (MemRegion_IsManaging(pCurNode, pAddress)) {
            return pCurNode;
        }
    });

    return NULL;
}

Bool Allocator_IsManaging(AllocatorRef _Nonnull pAllocator, Byte* _Nullable ptr)
{
    if (ptr == NULL || ptr == BYTE_PTR_MAX) {
        // Any allocator can take responsibility of that since deallocating these
        // things is a NOP anyway
        return true;
    }

    const Bool r = Allocator_GetMemRegionManaging_Locked(pAllocator, ptr) != NULL;
    return r;
}

ErrorCode Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, Int nbytes, Byte* _Nullable * _Nonnull pOutPtr)
{
    // Return the "empty memory block singleton" if the requested size is 0
    if (nbytes == 0) {
        *pOutPtr = BYTE_PTR_MAX;
        return EOK;
    }
    
    
    // Compute how many bytes we have to take from free memory
    const Int nBytesToAlloc = Int_RoundUpToPowerOf2(sizeof(MemBlock) + nbytes, HEAP_ALIGNMENT);
    
    
    // Note that the code here assumes desc 0 is chip RAM and all the others are
    // fast RAM. This is enforced by Allocator_Create().
    MemBlock* pMemBlock = NULL;
    decl_try_err();

    // Go through the available memory region trying to allocate the memory block
    // until it works.
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;
    while(pCurRegion != NULL) {
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
    *pOutPtr = (Byte*)pMemBlock + sizeof(MemBlock);
    return EOK;

catch:
    *pOutPtr = NULL;
    return err;
}

// Attempts to deallocate the given memory block. Returns EOK on success and
// ENOTBLK if the allocator does not manage the given memory block.
ErrorCode Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, Byte* _Nullable ptr)
{
    if (ptr == NULL || ptr == BYTE_PTR_MAX) {
        return EOK;
    }
    
    // Find out which memory region contains the block that we want to free
    MemRegion* pMemRegion = Allocator_GetMemRegionManaging_Locked(pAllocator, ptr);
    if (pMemRegion == NULL) {
        // 'ptr' isn't managed by this allocator
        return ENOTBLK;
    }
    
    MemBlock* pBlockToFree = (MemBlock*)(ptr - sizeof(MemBlock));
    
    
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

void Allocator_Dump(AllocatorRef _Nonnull pAllocator)
{
    print("Free list:\n");
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;
    while (pCurRegion != NULL) {
        MemBlock* pCurBlock = pCurRegion->first_free_block;

        while (pCurBlock) {
            print("   0x%p, %lu\n", ((Byte*)pCurBlock) + sizeof(MemBlock), pCurBlock->size - sizeof(MemBlock));
            pCurBlock = pCurBlock->next;
        }

        pCurRegion = (MemRegion*)pCurRegion->node.next;
    }
    
    print("\nAlloc list:\n");
    MemBlock* pCurBlock = pAllocator->first_allocated_block;
    while (pCurBlock) {
        Byte* pCurBlockBase = (Byte*)pCurBlock;
        MemRegion* pMemRegion = Allocator_GetMemRegionManaging_Locked(pAllocator, pCurBlockBase);
        
        print("   0x%p, %lu\n", pCurBlockBase + sizeof(MemBlock), pCurBlock->size - sizeof(MemBlock));
        pCurBlock = pCurBlock->next;
    }
    
    print("-------------------------------\n");
}

void Allocator_DumpMemoryRegions(AllocatorRef _Nonnull pAllocator)
{
    MemRegion* pCurRegion = (MemRegion*)pAllocator->regions.first;

    while (pCurRegion != NULL) {
        print("   lower: 0x%p, upper: 0x%p\n", pCurRegion->lower, pCurRegion->upper);

        pCurRegion = (MemRegion*)pCurRegion->node.next;
    }
}
