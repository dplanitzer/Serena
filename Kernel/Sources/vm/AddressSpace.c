//
//  AddressSpace.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "AddressSpace.h"
#include <dispatcher/Lock.h>
#include <hal/Platform.h>
#include <klib/List.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>


typedef struct MemEntry {
    char* _Nullable mem;
    size_t          size;
} MemEntry;

#define MEM_BLOCKS_CAPACITY 8
typedef struct MemBlocks {
    SListNode   node;
    size_t      count;  // Number of entries in use
    MemEntry    blocks[MEM_BLOCKS_CAPACITY];
} MemBlocks;


typedef struct AddressSpace {
    SList   mblocks;
    Lock    lock;
} AddressSpace;


errno_t AddressSpace_Create(AddressSpaceRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AddressSpaceRef self;

    try(kalloc_cleared(sizeof(AddressSpace), (void**) &self));
    SList_Init(&self->mblocks);
    Lock_Init(&self->lock);

    *pOutSelf = self;
    return EOK;

catch:
    AddressSpace_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void AddressSpace_Destroy(AddressSpaceRef _Nullable self)
{
    if (self) {
        MemBlocks* pCurMemBlocks = (MemBlocks*)self->mblocks.first;

        while (pCurMemBlocks) {
            MemBlocks* pNextMemBlocks = (MemBlocks*)pCurMemBlocks->node.next;

            for (int i = 0; i < pCurMemBlocks->count; i++) {
                kfree(pCurMemBlocks->blocks[i].mem);
                pCurMemBlocks->blocks[i].mem = NULL;
                pCurMemBlocks->blocks[i].size = 0;
            }

            SListNode_Deinit(&pCurMemBlocks->node);
            kfree(pCurMemBlocks);

            pCurMemBlocks = pNextMemBlocks;
        }
    }
}

bool AddressSpace_IsEmpty(AddressSpaceRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const bool isEmpty = SList_IsEmpty(&self->mblocks) || ((MemBlocks*) self->mblocks.first)->count == 0;
    Lock_Unlock(&self->lock);

    return isEmpty;
}

size_t AddressSpace_GetVirtualSize(AddressSpaceRef _Nonnull self)
{
    size_t siz = 0;

    Lock_Lock(&self->lock);
    SList_ForEach(&self->mblocks, MemBlocks,
        for (size_t i = 0; i < pCurNode->count; i++) {
            siz += pCurNode->blocks[i].size;
        }
    );
    Lock_Unlock(&self->lock);

    return siz;
}

// Allocates more address space to the calling process. The address space is
// expanded by 'nbytes' bytes. A pointer to the first byte in the newly allocated
// address space portion is return in 'pOutMem'. 'pOutMem' is set to NULL and a
// suitable error is returned if the allocation failed. 'nbytes' must be greater
// than 0 and a multiple of the CPU page size.
errno_t AddressSpace_Allocate(AddressSpaceRef _Nonnull self, ssize_t nbytes, void* _Nullable * _Nonnull pOutMem)
{
    decl_try_err();
    MemBlocks* pMemBlocks = NULL;
    char* pMem = NULL;

    if (nbytes == 0) {
        return EINVAL;
    }
    if (__Floor_PowerOf2(nbytes, CPU_PAGE_SIZE) != nbytes) {
        return EINVAL;
    }


    Lock_Lock(&self->lock);

    // We don't need to free the MemBlocks if the allocation of the memory block
    // below fails because we can always keep it around for the next allocation
    // request
    if (SList_IsEmpty(&self->mblocks)
        || ((MemBlocks*)self->mblocks.last)->count == MEM_BLOCKS_CAPACITY) {
        try(kalloc_cleared(sizeof(MemBlocks), (void**) &pMemBlocks));
        SListNode_Init(&pMemBlocks->node);
        SList_InsertAfterLast(&self->mblocks, &pMemBlocks->node);
    } else {
        pMemBlocks = (MemBlocks*)self->mblocks.last;
    }


    // Allocate the memory block
    try(kalloc(nbytes, (void**) &pMem));


    // Add the memory block to our list
    pMemBlocks->blocks[pMemBlocks->count].mem = pMem;
    pMemBlocks->blocks[pMemBlocks->count].size = nbytes;
    pMemBlocks->count++;

    Lock_Unlock(&self->lock);

    *pOutMem = pMem;
    return EOK;

catch:
    kfree(pMem);
    Lock_Unlock(&self->lock);
    *pOutMem = NULL;
    return err;
}
