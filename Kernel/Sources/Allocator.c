//
//  Allocator.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Allocator.h"
#include "Bytes.h"
#include "Lock.h"


#if __LP64__
#define HEAP_ALIGNMENT  16
#elif __LP32__
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
    Byte* _Nonnull      lower;
    Byte* _Nonnull      upper;
    MemBlock* _Nullable first_free_block;   // Every memory region has its own private free list
    UInt8               accessibility;      // MEM_ACCESS_XXX
    UInt8               reserved[3];
} MemRegion;


// An allocator manages memory from a pool of memory contiguous regions.
typedef struct _Allocator {
    Int                 regions_count;
    MemRegion* _Nonnull regions;
    MemBlock* _Nullable first_allocated_block;
    Lock                lock;
} Allocator;



AllocatorRef   gMainAllocator;


// Allocates a new heap. An allocator manages the memory regions described by the
// given memory descriptors. The heap managment data structures are stored inside
// those memory regions.
// \param pMemLayout the memory layout
// \return the heap
ErrorCode Allocator_Create(const MemoryLayout* _Nonnull pMemLayout, AllocatorRef _Nullable * _Nonnull pOutAllocator)
{
    assert(pMemLayout != NULL);
    
    // Validate some basic assumptions we make in the heap implementation to allow
    // for faster allocations:
    // desc 0 -> chip RAM
    // desc > 0 -> fast RAM
    assert(pMemLayout->descriptor_count > 0);
    assert(pMemLayout->descriptor[0].accessibility == (MEM_ACCESS_CHIPSET|MEM_ACCESS_CPU));
    for (Int i = 1; i < pMemLayout->descriptor_count; i++) {
        assert(pMemLayout->descriptor[i].accessibility == MEM_ACCESS_CPU);
    }
    

    // Reserve space for the heap structure. We put it preferably into the bottom
    // of fast RAM. If there is none then we put it at the bottom of chip RAM.
    Int idxOfMemRgnWithHeapStruct = (pMemLayout->descriptor_count > 1) ? 1 : 0;
    Byte* pAllocatorBase = align_up_byte_ptr(pMemLayout->descriptor[idxOfMemRgnWithHeapStruct].lower, HEAP_ALIGNMENT);
    MemBlock* pAllocedBlock = (MemBlock*)pAllocatorBase;
    AllocatorRef pAllocator = (AllocatorRef)(pAllocatorBase + sizeof(MemBlock));
    Byte* pHeapMemRegionsBase = align_up_byte_ptr(pAllocatorBase + sizeof(MemBlock) + sizeof(Allocator), HEAP_ALIGNMENT);
    
    pAllocedBlock->next = NULL;
    pAllocedBlock->size = pHeapMemRegionsBase - pAllocatorBase;
    
    pAllocator->regions_count = pMemLayout->descriptor_count;
    pAllocator->regions = (MemRegion*) pHeapMemRegionsBase;
    pAllocator->first_allocated_block = pAllocedBlock;
    Lock_Init(&pAllocator->lock);
    
    for (Int i = 0; i < pMemLayout->descriptor_count; i++) {
        pAllocator->regions[i].lower = pMemLayout->descriptor[i].lower;
        pAllocator->regions[i].upper = pMemLayout->descriptor[i].upper;
        pAllocator->regions[i].first_free_block = NULL;
        pAllocator->regions[i].accessibility = pMemLayout->descriptor[i].accessibility;
        pAllocedBlock->size += sizeof(MemRegion);
    }
    pAllocedBlock->size = UInt_RoundUpToPowerOf2(pAllocedBlock->size, HEAP_ALIGNMENT);
    
    
    // Create a free list for each memory region. Each region is covered by a
    // single free block at this point.
    for (Int i = 0; i < pAllocator->regions_count; i++) {
        const MemRegion* pCurMemRegion = &pAllocator->regions[i];
        Byte* pFreeLower;
        
        if (i == idxOfMemRgnWithHeapStruct) {
            pFreeLower = pCurMemRegion->lower + pAllocedBlock->size;
        } else {
            pFreeLower = align_up_byte_ptr(pCurMemRegion->lower, HEAP_ALIGNMENT);
        }

        MemBlock* pFreeBlock = (MemBlock*)pFreeLower;
        pFreeBlock->next = NULL;
        pFreeBlock->size = pCurMemRegion->upper - pFreeLower;
        pAllocator->regions[i].first_free_block = pFreeBlock;
    }
    
    *pOutAllocator = pAllocator;
    return EOK;
}

// Returns the correct memory region access mode for the given heap options.
// Assumes CPU access if no explicit access options were specified.
// Note that chipset access always also implies CPU access on the Amiga.
static UInt8 mem_access_mode_from_options(Int options)
{
    UInt8 access = 0;
    
    if ((options & ALLOCATOR_OPTION_CPU) != 0) {
        access |= MEM_ACCESS_CPU;
    }
    if ((options & ALLOCATOR_OPTION_CHIPSET) != 0) {
        access |= MEM_ACCESS_CHIPSET;
    }
    
    if (access == 0) {
        access |= MEM_ACCESS_CPU;
    }
    if (access == MEM_ACCESS_CHIPSET) {
        access |= MEM_ACCESS_CPU;
    }
    
    return access;
}

// Allocates 'nBytesToAlloc' from the memory region at index 'idxOfMemRgn'. Note
// that 'nBytesToAlloc' has to include the heap block header and the correct
// alignment.
static Byte* _Nullable Allocator_AllocateBytesFromMemoryRegion(AllocatorRef _Nonnull pAllocator, Int idxOfMemRgn, Int nBytesToAlloc)
{
    // first fit search
    MemBlock* pPrevBlock = NULL;
    MemBlock* pCurBlock = pAllocator->regions[idxOfMemRgn].first_free_block;
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
    
    // Save the pointer to the next free block (because we may override this pointer below)
    MemBlock* pNextFreeBlock = pCurBlock->next;
    
    
    // Split the existing free block into an allocated block and a new
    // (and smaller) free block
    UInt nNewFreeBytes = pCurBlock->size - nBytesToAlloc;
    MemBlock* pAllocedBlock = pCurBlock;
    MemBlock* pNewFreeBlock = (MemBlock*)((Byte*)pCurBlock + nBytesToAlloc);
    
    
    // Initialize the allocated block and add it to the allocated block list
    pAllocedBlock->next = pAllocator->first_allocated_block;
    pAllocedBlock->size = nBytesToAlloc;
    pAllocator->first_allocated_block = pAllocedBlock;
    
    
    // Initialize the new free block and replace the old one in the free list with this new one
    pNewFreeBlock->next = pNextFreeBlock;
    pNewFreeBlock->size = nNewFreeBytes;
    if (pPrevBlock) {
        pPrevBlock->next = pNewFreeBlock;
    } else {
        pAllocator->regions[idxOfMemRgn].first_free_block = pNewFreeBlock;
    }
    
    
    // Return the user pointer to the allocated memory
    return (Byte*)pAllocedBlock + sizeof(MemBlock);
}

ErrorCode Allocator_AllocateBytes(AllocatorRef _Nonnull pAllocator, Int nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr)
{
    // Return the "empty memory block singleton" if the requested size is 0
    if (nbytes == 0) {
        *pOutPtr = BYTE_PTR_MAX;
        return EOK;
    }
    
    
    // Derive the memory region access mode from the 'options'
    UInt8 access = mem_access_mode_from_options(options);
    
    
    // Compute how many bytes we have to take from free memory
    const Int nBytesToAlloc = Int_RoundUpToPowerOf2(sizeof(MemBlock) + nbytes, HEAP_ALIGNMENT);
    
    
    // NOte that the code here assumes desc 0 is chip RAM and all the others are
    // fast RAM. This is enforced by Heap_Create().
    Byte* ptr = NULL;
    ErrorCode err;

    if ((err = Lock_Lock(&pAllocator->lock)) != EOK) {
        *pOutPtr = NULL;
        return err; 
    }

    if (access == MEM_ACCESS_CPU) {
        for (Int i = 1; i < pAllocator->regions_count; i++) {
            ptr = Allocator_AllocateBytesFromMemoryRegion(pAllocator, i, nBytesToAlloc);
            if (ptr) {
                break;
            }
        }
        
        if (ptr == NULL) {
            // Fall back to chip RAM 'cause there's no fast RAM or it's exhausted
            ptr = Allocator_AllocateBytesFromMemoryRegion(pAllocator, 0, nBytesToAlloc);
        }
    }
    else {
        ptr = Allocator_AllocateBytesFromMemoryRegion(pAllocator, 0, nBytesToAlloc);
    }

    Lock_Unlock(&pAllocator->lock);

    
    // Zero the memory if requested
    if (ptr && (options & ALLOCATOR_OPTION_CLEAR) != 0) {
        Bytes_ClearRange(ptr, nbytes);
    }
    
    
    // Return the allocated bytes
    *pOutPtr = ptr;
    return EOK;
}

ErrorCode Allocator_AllocateBytesAt(AllocatorRef _Nonnull pAllocator, Byte* _Nonnull pAddr, Int nbytes)
{
    assert(pAddr != NULL);
    assert(nbytes > 0);
    assert(align_up_byte_ptr(pAddr, HEAP_ALIGNMENT) == pAddr);
    
    
    // Compute how many bytes we have to take from free memory
    const Int nBytesToAlloc = Int_RoundUpToPowerOf2(sizeof(MemBlock) + nbytes, HEAP_ALIGNMENT);
    
    
    // Compute the block lower and upper bounds
    ErrorCode err;

    if ((err = Lock_Lock(&pAllocator->lock)) != EOK) {
        return err;
    }

    Byte* pBlockLower = pAddr - sizeof(MemBlock);
    Byte* pBlockUpper = pBlockLower + nBytesToAlloc;
    
    
    // Find out which memory region contains the block that we want to allocate.
    // Return out of memory if no memory region fully contains the requested block.
    Int idxOfMemRgn = -1;
    
    for (Int i = 0; i < pAllocator->regions_count; i++) {
        const Byte* pMemLower = pAllocator->regions[i].lower;
        const Byte* pMemUpper = pAllocator->regions[i].upper;
        
        if (pBlockLower >= pMemLower && pBlockUpper <= pMemUpper) {
            idxOfMemRgn =i;
            break;
        }
    }
    
    if (idxOfMemRgn == -1) {
        goto failed;
    }
    
    
    // Find the free block which contains the requested byte range
    MemBlock* pPrevBlock = NULL;
    MemBlock* pCurBlock = pAllocator->regions[idxOfMemRgn].first_free_block;
    MemBlock* pFoundBlock = NULL;
    while (pCurBlock) {
        Byte* pCurLower = (Byte*) pCurBlock;
        Byte* pCurUpper = pCurLower + pCurBlock->size;
        
        if (pBlockLower >= pCurLower && pBlockUpper <= pCurUpper) {
            pFoundBlock = pCurBlock;
            break;
        }
        
        pPrevBlock = pCurBlock;
        pCurBlock = pCurBlock->next;
    }
    
    if (pFoundBlock == NULL) {
        goto failed;
    }
    
    
    // Okay we found the free block which contains the requested range. Carve out
    // the requested range. This means that we may cut off bytes from the start or
    // the end or we have to split the free block.
    Byte* pFoundLower = (Byte*)pFoundBlock;
    Byte* pFoundUpper = pFoundLower + pFoundBlock->size;
    
    if (pFoundLower == pBlockLower) {
        // Cut bytes off from the bottom of the free block
        MemBlock* pNewFreeBlock = (MemBlock*) (pBlockLower + nBytesToAlloc);
        
        pNewFreeBlock->next = pFoundBlock->next;
        pNewFreeBlock->size = pFoundBlock->size - nBytesToAlloc;
        if (pPrevBlock) {
            pPrevBlock->next = pNewFreeBlock;
        } else {
            pAllocator->regions[idxOfMemRgn].first_free_block = pNewFreeBlock;
        }
    }
    else if (pFoundUpper == pBlockUpper) {
        // Cut bytes off from the top of the free block
        pFoundBlock->size -= nBytesToAlloc;
    }
    else {
        // Split the found free block into a new lower and upper free block
        MemBlock* pNewUpperFreeBlock = (MemBlock*)pBlockUpper;
        
        pNewUpperFreeBlock->size = pFoundUpper - pBlockUpper;
        pNewUpperFreeBlock->next = pFoundBlock->next;
        
        pFoundBlock->size = pBlockLower - pFoundLower;
        pFoundBlock->next = pNewUpperFreeBlock;
    }
    
    
    // Create the allocated block header and add it to the allocated block list
    MemBlock* pAllocedBlock = (MemBlock*)pBlockLower;
    
    pAllocedBlock->size = nBytesToAlloc;
    pAllocedBlock->next = pAllocator->first_allocated_block;
    pAllocator->first_allocated_block = pAllocedBlock;
    
    Lock_Unlock(&pAllocator->lock);

    return EOK;
    
failed:
    Lock_Unlock(&pAllocator->lock);
    return ENOMEM;
}

void Allocator_DeallocateBytes(AllocatorRef _Nonnull pAllocator, Byte* _Nullable ptr)
{
    if (ptr == NULL || ptr == BYTE_PTR_MAX) {
        return;
    }
    
    Lock_Lock(&pAllocator->lock);

    // Find out which memory region contains the block that we want to free
    Int idxOfMemRgn = -1;
    
    for (Int i = 0; i < pAllocator->regions_count; i++) {
        const Byte* pMemLower = pAllocator->regions[i].lower;
        const Byte* pMemUpper = pAllocator->regions[i].upper;
        
        if (ptr >= pMemLower && ptr <= pMemUpper) {
            idxOfMemRgn = i;
        }
    }
    
    assert(idxOfMemRgn != -1);
    
    MemBlock* pBlockToFree = (MemBlock*)(ptr - sizeof(MemBlock));
    
    
    // Remove the allocated block from the list of allocated blocks
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
    
    assert(pBlockToFree->next == NULL);
    
    
    // Compute the lower and the upper bound of the block that we want to free.
    Byte* pLowerToFree = (Byte*)pBlockToFree;
    Byte* pUpperToFree = pLowerToFree + pBlockToFree->size;
    
    
    // Go through the free list and find the block that is right below the block
    // we want to free and th eblock that is right above the block we want to free.
    // We'll then merge everything into the lkowest block and we remove the highest
    // block from the free list. That's the simplest way to do things.
    // NOTE: an allocated block may be bordered by a free block on both sides!
    MemBlock* pUpperPrevFreeBlock = NULL;
    MemBlock* pLowerPrevFreeBlock = NULL;
    MemBlock* pUpperFreeBlock = NULL;
    MemBlock* pLowerFreeBlock = NULL;
    pPrevBlock = NULL;
    pCurBlock = pAllocator->regions[idxOfMemRgn].first_free_block;
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
        // Case 1: adjacent to lower free block -> merge everything into the lower free block
        pLowerFreeBlock->size += pBlockToFree->size;
        pLowerFreeBlock->size += pUpperFreeBlock->size;
        
        if (pUpperPrevFreeBlock) {
            pUpperPrevFreeBlock->next = pUpperFreeBlock->next;
        } else {
            pAllocator->regions[idxOfMemRgn].first_free_block = pUpperFreeBlock->next;
        }
        
        pUpperFreeBlock->next = NULL;
        pUpperFreeBlock->size = 0;
        pBlockToFree->next = NULL;
        pBlockToFree->size = 0;
    }
    else if (pUpperFreeBlock) {
        // Case 2: adjacent to upper free block -> merge everything into the block we want to free
        pBlockToFree->size += pUpperFreeBlock->size;
        
        pBlockToFree->next = pUpperFreeBlock->next;
        if (pUpperPrevFreeBlock) {
            pUpperPrevFreeBlock->next = pBlockToFree;
        } else {
            pAllocator->regions[idxOfMemRgn].first_free_block = pBlockToFree;
        }
        
        pUpperFreeBlock->next = NULL;
        pUpperFreeBlock->size = 0;
    }
    else {
        // Case 3: no adjacent free block -> add the block to free as is to the free list
        pBlockToFree->next = pAllocator->regions[idxOfMemRgn].first_free_block;
        pAllocator->regions[idxOfMemRgn].first_free_block = pBlockToFree;
    }
    
    Lock_Unlock(&pAllocator->lock);
}

void Allocator_Dump(AllocatorRef _Nonnull pAllocator)
{
    Lock_Lock(&pAllocator->lock);

    print("Free list:\n");
    for (Int i = 0; i < pAllocator->regions_count; i++) {
        MemBlock* pCurBlock = pAllocator->regions[i].first_free_block;
        Byte* pCurBlockBase = (Byte*)pCurBlock;
        const Character* ramType = (pCurBlockBase >= pAllocator->regions[0].lower && pCurBlockBase < pAllocator->regions[0].upper) ? "CHIP" : "FAST";

        while (pCurBlock) {
            print("   0x%p, %lu  %s\n", pCurBlockBase + sizeof(MemBlock), pCurBlock->size - sizeof(MemBlock), ramType);
            pCurBlock = pCurBlock->next;
        }
    }
    
    print("\nAlloc list:\n");
    MemBlock* pCurBlock = pAllocator->first_allocated_block;
    while (pCurBlock) {
        Byte* pCurBlockBase = (Byte*)pCurBlock;
        const Character* ramType = (pCurBlockBase >= pAllocator->regions[0].lower && pCurBlockBase < pAllocator->regions[0].upper) ? "CHIP" : "FAST";
        
        print("   0x%p, %lu  %s\n", pCurBlockBase + sizeof(MemBlock), pCurBlock->size - sizeof(MemBlock), ramType);
        pCurBlock = pCurBlock->next;
    }
    
    print("-------------------------------\n");
    
    Lock_Unlock(&pAllocator->lock);
}
