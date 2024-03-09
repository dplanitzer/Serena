//
//  AddressSpace.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "AddressSpace.h"
#include <dispatcher/Lock.h>


#define MEM_BLOCKS_CAPACITY 8
typedef struct _MemBlocks {
    SListNode       node;
    size_t          count;  // Number of entries in use
    char* _Nullable blocks[MEM_BLOCKS_CAPACITY];
} MemBlocks;


typedef struct _AddressSpace {
    SList   mblocks;
    Lock    lock;
} AddressSpace;


errno_t AddressSpace_Create(AddressSpaceRef _Nullable * _Nonnull pOutSpace)
{
    decl_try_err();
    AddressSpaceRef pSpace;

    try(kalloc_cleared(sizeof(AddressSpace), (void**) &pSpace));
    SList_Init(&pSpace->mblocks);
    Lock_Init(&pSpace->lock);

    *pOutSpace = pSpace;
    return EOK;

catch:
    AddressSpace_Destroy(pSpace);
    *pOutSpace = NULL;
    return err;
}

void AddressSpace_Destroy(AddressSpaceRef _Nullable pSpace)
{
    if (pSpace) {
        MemBlocks* pCurMemBlocks = (MemBlocks*)pSpace->mblocks.first;

        while (pCurMemBlocks) {
            MemBlocks* pNextMemBlocks = (MemBlocks*)pCurMemBlocks->node.next;

            for (int i = 0; i < pCurMemBlocks->count; i++) {
                kfree(pCurMemBlocks->blocks[i]);
                pCurMemBlocks->blocks[i] = NULL;
            }

            SListNode_Deinit(&pCurMemBlocks->node);
            kfree(pCurMemBlocks);

            pCurMemBlocks = pNextMemBlocks;
        }
    }
}

bool AddressSpace_IsEmpty(AddressSpaceRef _Nonnull pSpace)
{
    Lock_Lock(&pSpace->lock);
    const bool isEmpty = SList_IsEmpty(&pSpace->mblocks) || ((MemBlocks*) pSpace->mblocks.first)->count == 0;
    Lock_Unlock(&pSpace->lock);

    return isEmpty;
}

// Allocates more address space to the calling process. The address space is
// expanded by 'count' bytes. A pointer to the first byte in the newly allocated
// address space portion is return in 'pOutMem'. 'pOutMem' is set to NULL and a
// suitable error is returned if the allocation failed. 'count' must be greater
// than 0 and a multiple of the CPU page size.
errno_t AddressSpace_Allocate(AddressSpaceRef _Nonnull pSpace, ssize_t count, void* _Nullable * _Nonnull pOutMem)
{
    decl_try_err();
    MemBlocks* pMemBlocks = NULL;
    char* pMem = NULL;

    if (count == 0) {
        return EINVAL;
    }
    if (__Floor_PowerOf2(count, CPU_PAGE_SIZE) != count) {
        return EINVAL;
    }


    Lock_Lock(&pSpace->lock);

    // We don't need to free the MemBlocks if the allocation of the memory block
    // below fails because we can always keep it around for the next allocation
    // request
    if (SList_IsEmpty(&pSpace->mblocks)
        || ((MemBlocks*)pSpace->mblocks.last)->count == MEM_BLOCKS_CAPACITY) {
        try(kalloc_cleared(sizeof(MemBlocks), (void**) &pMemBlocks));
        SListNode_Init(&pMemBlocks->node);
        SList_InsertAfterLast(&pSpace->mblocks, &pMemBlocks->node);
    } else {
        pMemBlocks = (MemBlocks*)pSpace->mblocks.last;
    }


    // Allocate the memory block
    try(kalloc(count, (void**) &pMem));


    // Add the memory block to our list
    pMemBlocks->blocks[pMemBlocks->count++] = pMem;

    Lock_Unlock(&pSpace->lock);

    *pOutMem = pMem;
    return EOK;

catch:
    kfree(pMem);
    Lock_Unlock(&pSpace->lock);
    *pOutMem = NULL;
    return err;
}
