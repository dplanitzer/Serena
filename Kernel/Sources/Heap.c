//
//  Heap.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Heap.h"
#include "Bytes.h"


#if __LP64__
#define HEAP_ALIGNMENT  16
#elif __LP32__
#define HEAP_ALIGNMENT  8
#else
#error "don't know how to align heap blocks"
#endif


// Allocates uninitialized CPU-accessible memory from the kernel heap. Returns
// NULL if the memory could not be allocated. The returned memory is not
// necessarily accessibe to I/O DMA operations. Use kalloc_options() with a
// suitable option if DMA accessability is desired.
ErrorCode kalloc(Int nbytes, Byte* _Nullable * _Nonnull pOutPtr)
{
    return Heap_AllocateBytes(gHeap, nbytes, HEAP_ALLOC_OPTION_CPU, pOutPtr);
}

// Same as kalloc() but allocated memory that is filled with zeros.
ErrorCode kalloc_cleared(Int nbytes, Byte* _Nullable * _Nonnull pOutPtr)
{
    return Heap_AllocateBytes(gHeap, nbytes, HEAP_ALLOC_OPTION_CLEAR | HEAP_ALLOC_OPTION_CPU, pOutPtr);
}

// Allocates memory from the kernel heap. Returns NULL if the memory could not be
// allocated. 'options' is a combination of the HEAP_ALLOC_OPTION_XXX flags.
ErrorCode kalloc_options(Int nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr)
{
    return Heap_AllocateBytes(gHeap, nbytes, options, pOutPtr);
}

// Frees kernel memory allocated with the kalloc() function.
void kfree(Byte* _Nullable ptr)
{
    Heap_DeallocateBytes(gHeap, ptr);
}


Heap*   gHeap;

// Allocates a new heap. The heap manages the memory regions described by the given
// memory descriptors. The heap managment data structures are stored inside those
// memory regions.
// \param pMemDescs the memory descriptors
// \param nMemDescs the number of memory descriptors
// \return the heap
ErrorCode Heap_Create(const MemoryDescriptor* _Nonnull pMemDescs, Int nMemDescs, Heap* _Nullable * _Nonnull pOutHeap)
{
    assert(pMemDescs != NULL);
    
    // Validate some basic assumptions we make in the heap implementation to allow
    // for faster allocations:
    // desc 0 -> chip RAM
    // desc > 0 -> fast RAM
    assert(nMemDescs > 0);
    assert(pMemDescs[0].accessibility == (MEM_ACCESS_CHIPSET|MEM_ACCESS_CPU));
    for (Int i = 1; i < nMemDescs; i++) {
        assert(pMemDescs[i].accessibility == MEM_ACCESS_CPU);
    }
    

    // Reserve space for the heap structure. We put it preferably into the bottom
    // of fast RAM. If there is none then we put it at the bottom of chip RAM
    // (just above lowmem).
    Int idxOfMemRgnWithHeapStruct = (nMemDescs > 1) ? 1 : 0;
    Byte* pHeapBase = align_up_byte_ptr(pMemDescs[idxOfMemRgnWithHeapStruct].lower, HEAP_ALIGNMENT);
    Heap_Block* pAllocedBlock = (Heap_Block*)pHeapBase;
    Heap* pHeap = (Heap*)(pHeapBase + sizeof(Heap_Block));
    Byte* pHeapMemDescsBase = align_up_byte_ptr(pHeapBase + sizeof(Heap_Block) + sizeof(Heap), HEAP_ALIGNMENT);
    
    pAllocedBlock->next = NULL;
    pAllocedBlock->size = pHeapMemDescsBase - pHeapBase;
    
    pHeap->descriptors_count = nMemDescs;
    pHeap->descriptors = (Heap_MemoryDescriptor*) pHeapMemDescsBase;
    pHeap->first_allocated_block = pAllocedBlock;
    Lock_Init(&pHeap->lock);
    
    for (Int i = 0; i < nMemDescs; i++) {
        pHeap->descriptors[i].lower = pMemDescs[i].lower;
        pHeap->descriptors[i].upper = pMemDescs[i].upper;
        pHeap->descriptors[i].first_free_block = NULL;
        pHeap->descriptors[i].accessibility = pMemDescs[i].accessibility;
        pAllocedBlock->size += sizeof(Heap_MemoryDescriptor);
    }
    pAllocedBlock->size = UInt_RoundUpToPowerOf2(pAllocedBlock->size, HEAP_ALIGNMENT);
    
    
    // Create a free list for each memory region. Each region is covered by a
    // single free block at this point.
    for (Int i = 0; i < pHeap->descriptors_count; i++) {
        const Heap_MemoryDescriptor* pCurMemDesc = &pHeap->descriptors[i];
        Byte* pFreeLower;
        
        if (i == idxOfMemRgnWithHeapStruct) {
            pFreeLower = pCurMemDesc->lower + pAllocedBlock->size;
        } else {
            pFreeLower = align_up_byte_ptr(pCurMemDesc->lower, HEAP_ALIGNMENT);
        }

        Heap_Block* pFreeBlock = (Heap_Block*)pFreeLower;
        pFreeBlock->next = NULL;
        pFreeBlock->size = pCurMemDesc->upper - pFreeLower;
        pHeap->descriptors[i].first_free_block = pFreeBlock;
    }
    
    *pOutHeap = pHeap;
    return EOK;
}

// Returns the correct memory region access mode for the given heap options.
// Assumes CPU access if no explicit access options were specified.
// Note that chipset access always also implies CPU access on the Amiga.
static UInt8 mem_access_mode_from_options(Int options)
{
    UInt8 access = 0;
    
    if ((options & HEAP_ALLOC_OPTION_CPU) != 0) {
        access |= MEM_ACCESS_CPU;
    }
    if ((options & HEAP_ALLOC_OPTION_CHIPSET) != 0) {
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
static Byte* _Nullable Heap_AllocateBytesFromMemoryRegion(Heap* _Nonnull pHeap, Int idxOfMemRgn, Int nBytesToAlloc)
{
    // first fit search
    Heap_Block* pPrevBlock = NULL;
    Heap_Block* pCurBlock = pHeap->descriptors[idxOfMemRgn].first_free_block;
    Heap_Block* pFoundBlock = NULL;
    
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
    Heap_Block* pNextFreeBlock = pCurBlock->next;
    
    
    // Split the existing free block into an allocated block and a new
    // (and smaller) free block
    UInt nNewFreeBytes = pCurBlock->size - nBytesToAlloc;
    Heap_Block* pAllocedBlock = pCurBlock;
    Heap_Block* pNewFreeBlock = (Heap_Block*)((Byte*)pCurBlock + nBytesToAlloc);
    
    
    // Initialize the allocated block and add it to the allocated block list
    pAllocedBlock->next = pHeap->first_allocated_block;
    pAllocedBlock->size = nBytesToAlloc;
    pHeap->first_allocated_block = pAllocedBlock;
    
    
    // Initialize the new free block and replace the old one in the free list with this new one
    pNewFreeBlock->next = pNextFreeBlock;
    pNewFreeBlock->size = nNewFreeBytes;
    if (pPrevBlock) {
        pPrevBlock->next = pNewFreeBlock;
    } else {
        pHeap->descriptors[idxOfMemRgn].first_free_block = pNewFreeBlock;
    }
    
    
    // Return the user pointer to the allocated memory
    return (Byte*)pAllocedBlock + sizeof(Heap_Block);
}

ErrorCode Heap_AllocateBytes(Heap* _Nonnull pHeap, Int nbytes, UInt options, Byte* _Nullable * _Nonnull pOutPtr)
{
    // Return the "empty memory block singleton" if the requested size is 0
    if (nbytes == 0) {
        *pOutPtr = BYTE_PTR_MAX;
        return EOK;
    }
    
    
    // Derive the memory region access mode from the 'options'
    UInt8 access = mem_access_mode_from_options(options);
    
    
    // Compute how many bytes we have to take from free memory
    const Int nBytesToAlloc = Int_RoundUpToPowerOf2(sizeof(Heap_Block) + nbytes, HEAP_ALIGNMENT);
    
    
    // NOte that the code here assumes desc 0 is chip RAM and all the others are
    // fast RAM. This is enforced by Heap_Create().
    Byte* ptr = NULL;
    ErrorCode err;

    if ((err = Lock_Lock(&pHeap->lock)) != EOK) {
        *pOutPtr = NULL;
        return err; 
    }

    if (access == MEM_ACCESS_CPU) {
        for (Int i = 1; i < pHeap->descriptors_count; i++) {
            ptr = Heap_AllocateBytesFromMemoryRegion(pHeap, i, nBytesToAlloc);
            if (ptr) {
                break;
            }
        }
        
        if (ptr == NULL) {
            // Fall back to chip RAM 'cause ther's no fast RAM or it's exhausted
            ptr = Heap_AllocateBytesFromMemoryRegion(pHeap, 0, nBytesToAlloc);
        }
    }
    else {
        ptr = Heap_AllocateBytesFromMemoryRegion(pHeap, 0, nBytesToAlloc);
    }

    Lock_Unlock(&pHeap->lock);

    
    // Zero the memory if requested
    if (ptr && (options & HEAP_ALLOC_OPTION_CLEAR) != 0) {
        Bytes_ClearRange(ptr, nbytes);
    }
    
    
    // Return the allocated bytes
    *pOutPtr = ptr;
    return EOK;
}

ErrorCode Heap_AllocateBytesAt(Heap* _Nonnull pHeap, Byte* _Nonnull pAddr, Int nbytes)
{
    assert(pAddr != NULL);
    assert(nbytes > 0);
    assert(align_up_byte_ptr(pAddr, HEAP_ALIGNMENT) == pAddr);
    
    
    // Compute how many bytes we have to take from free memory
    const Int nBytesToAlloc = Int_RoundUpToPowerOf2(sizeof(Heap_Block) + nbytes, HEAP_ALIGNMENT);
    
    
    // Compute the block lower and upper bounds
    ErrorCode err;

    if ((err = Lock_Lock(&pHeap->lock)) != EOK) {
        return err;
    }

    Byte* pBlockLower = pAddr - sizeof(Heap_Block);
    Byte* pBlockUpper = pBlockLower + nBytesToAlloc;
    
    
    // Find out which memory region contains the block that we want to allocate.
    // Return out of memory if no memory region fully contains the requested block.
    Int idxOfMemRgn = -1;
    
    for (Int i = 0; i < pHeap->descriptors_count; i++) {
        const Byte* pMemLower = pHeap->descriptors[i].lower;
        const Byte* pMemUpper = pHeap->descriptors[i].upper;
        
        if (pBlockLower >= pMemLower && pBlockUpper <= pMemUpper) {
            idxOfMemRgn =i;
            break;
        }
    }
    
    if (idxOfMemRgn == -1) {
        goto failed;
    }
    
    
    // Find the free block which contains the requested byte range
    Heap_Block* pPrevBlock = NULL;
    Heap_Block* pCurBlock = pHeap->descriptors[idxOfMemRgn].first_free_block;
    Heap_Block* pFoundBlock = NULL;
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
        Heap_Block* pNewFreeBlock = (Heap_Block*) (pBlockLower + nBytesToAlloc);
        
        pNewFreeBlock->next = pFoundBlock->next;
        pNewFreeBlock->size = pFoundBlock->size - nBytesToAlloc;
        if (pPrevBlock) {
            pPrevBlock->next = pNewFreeBlock;
        } else {
            pHeap->descriptors[idxOfMemRgn].first_free_block = pNewFreeBlock;
        }
    }
    else if (pFoundUpper == pBlockUpper) {
        // Cut bytes off from the top of the free block
        pFoundBlock->size -= nBytesToAlloc;
    }
    else {
        // Split the found free block into a new lower and upper free block
        Heap_Block* pNewUpperFreeBlock = (Heap_Block*)pBlockUpper;
        
        pNewUpperFreeBlock->size = pFoundUpper - pBlockUpper;
        pNewUpperFreeBlock->next = pFoundBlock->next;
        
        pFoundBlock->size = pBlockLower - pFoundLower;
        pFoundBlock->next = pNewUpperFreeBlock;
    }
    
    
    // Create the allocated block header and add it toi the allocated block list
    Heap_Block* pAllocedBlock = (Heap_Block*)pBlockLower;
    
    pAllocedBlock->size = nBytesToAlloc;
    pAllocedBlock->next = pHeap->first_allocated_block;
    pHeap->first_allocated_block = pAllocedBlock;
    
    Lock_Unlock(&pHeap->lock);

    return EOK;
    
failed:
    Lock_Unlock(&pHeap->lock);
    return ENOMEM;
}

void Heap_DeallocateBytes(Heap* _Nonnull pHeap, Byte* _Nullable ptr)
{
    if (ptr == NULL || ptr == BYTE_PTR_MAX) {
        return;
    }
    
    Lock_Lock(&pHeap->lock);

    // Find out which memory region contains the block that we want to free
    Int idxOfMemRgn = -1;
    
    for (Int i = 0; i < pHeap->descriptors_count; i++) {
        const Byte* pMemLower = pHeap->descriptors[i].lower;
        const Byte* pMemUpper = pHeap->descriptors[i].upper;
        
        if (ptr >= pMemLower && ptr <= pMemUpper) {
            idxOfMemRgn = i;
        }
    }
    
    assert(idxOfMemRgn != -1);
    
    Heap_Block* pBlockToFree = (Heap_Block*)(ptr - sizeof(Heap_Block));
    
    
    // Remove the allocated block from the list of allocated blocks
    Heap_Block* pPrevBlock = NULL;
    Heap_Block* pCurBlock = pHeap->first_allocated_block;
    while (pCurBlock) {
        if (pCurBlock == pBlockToFree) {
            if (pPrevBlock) {
                pPrevBlock->next = pBlockToFree->next;
            } else {
                pHeap->first_allocated_block = pBlockToFree->next;
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
    Heap_Block* pUpperPrevFreeBlock = NULL;
    Heap_Block* pLowerPrevFreeBlock = NULL;
    Heap_Block* pUpperFreeBlock = NULL;
    Heap_Block* pLowerFreeBlock = NULL;
    pPrevBlock = NULL;
    pCurBlock = pHeap->descriptors[idxOfMemRgn].first_free_block;
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
            pHeap->descriptors[idxOfMemRgn].first_free_block = pUpperFreeBlock->next;
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
            pHeap->descriptors[idxOfMemRgn].first_free_block = pBlockToFree;
        }
        
        pUpperFreeBlock->next = NULL;
        pUpperFreeBlock->size = 0;
    }
    else {
        // Case 3: no adjacent free block -> add the block to free as is to the free list
        pBlockToFree->next = pHeap->descriptors[idxOfMemRgn].first_free_block;
        pHeap->descriptors[idxOfMemRgn].first_free_block = pBlockToFree;
    }
    
    Lock_Unlock(&pHeap->lock);
}

void Heap_Dump(Heap* _Nonnull pHeap)
{
    Lock_Lock(&pHeap->lock);

    print("Free list:\n");
    for (Int i = 0; i < pHeap->descriptors_count; i++) {
        Heap_Block* pCurBlock = pHeap->descriptors[i].first_free_block;
        Byte* pCurBlockBase = (Byte*)pCurBlock;
        const Character* ramType = (pCurBlockBase >= pHeap->descriptors[0].lower && pCurBlockBase < pHeap->descriptors[0].upper) ? "CHIP" : "FAST";

        while (pCurBlock) {
            print("   0x%p, %lu  %s\n", pCurBlockBase + sizeof(Heap_Block), pCurBlock->size - sizeof(Heap_Block), ramType);
            pCurBlock = pCurBlock->next;
        }
    }
    
    print("\nAlloc list:\n");
    Heap_Block* pCurBlock = pHeap->first_allocated_block;
    while (pCurBlock) {
        Byte* pCurBlockBase = (Byte*)pCurBlock;
        const Character* ramType = (pCurBlockBase >= pHeap->descriptors[0].lower && pCurBlockBase < pHeap->descriptors[0].upper) ? "CHIP" : "FAST";
        
        print("   0x%p, %lu  %s\n", pCurBlockBase + sizeof(Heap_Block), pCurBlock->size - sizeof(Heap_Block), ramType);
        pCurBlock = pCurBlock->next;
    }
    
    print("-------------------------------\n");
    
    Lock_Unlock(&pHeap->lock);
}
