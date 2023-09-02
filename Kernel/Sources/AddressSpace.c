//
//  AddressSpace.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "AddressSpace.h"
#include "Lock.h"


#define MEM_BLOCKS_CAPACITY 8
typedef struct _MemBlocks {
    SListNode       node;
    Int             count;  // Number of entries in use
    Byte* _Nullable blocks[MEM_BLOCKS_CAPACITY];
} MemBlocks;


typedef struct _AddressSpace {
    SList   mblocks;
    Lock    lock;
} AddressSpace;


ErrorCode AddressSpace_Create(AddressSpaceRef _Nullable * _Nonnull pOutSpace)
{
    decl_try_err();
    AddressSpaceRef pSpace;

    try(kalloc_cleared(sizeof(AddressSpace), (Byte**) &pSpace));
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

            for (Int i = 0; i < pCurMemBlocks->count; i++) {
                kfree(pCurMemBlocks->blocks[i]);
                pCurMemBlocks->blocks[i] = NULL;
            }

            SListNode_Deinit(&pCurMemBlocks->node);
            kfree((Byte*) pCurMemBlocks);

            pCurMemBlocks = pNextMemBlocks;
        }
    }
}

Bool AddressSpace_IsEmpty(AddressSpaceRef _Nonnull pSpace)
{
    Lock_Lock(&pSpace->lock);
    const Bool isEmpty = SList_IsEmpty(&pSpace->mblocks) || ((MemBlocks*) pSpace->mblocks.first)->count == 0;
    Lock_Unlock(&pSpace->lock);

    return isEmpty;
}

ErrorCode AddressSpace_Allocate(AddressSpaceRef _Nonnull pSpace, Int nbytes, Byte* _Nullable * _Nonnull pOutMem)
{
    decl_try_err();
    MemBlocks* pMemBlocks = NULL;
    Byte* pMem = NULL;

    if (nbytes < 0) {
        return EPARAM;
    }
    if (nbytes == 0) {
        return EOK;
    }


    Lock_Lock(&pSpace->lock);

    // We don't need to free the MemBlocks if the allocation of the memory block
    // below fails because we can always keep it around for the next allocation
    // request
    if (SList_IsEmpty(&pSpace->mblocks)
        || ((MemBlocks*)pSpace->mblocks.last)->count == MEM_BLOCKS_CAPACITY) {
        try(kalloc_cleared(sizeof(MemBlocks), (Byte**) &pMemBlocks));
        SListNode_Init(&pMemBlocks->node);
        SList_InsertAfterLast(&pSpace->mblocks, &pMemBlocks->node);
    } else {
        pMemBlocks = (MemBlocks*)pSpace->mblocks.last;
    }


    // Allocate the memory block
    try(kalloc(nbytes, &pMem));


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
