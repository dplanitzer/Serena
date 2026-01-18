//
//  AddressSpace.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "AddressSpace.h"
#include <hal/cpu.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>


typedef struct MemEntry {
    char* _Nullable mem;
    size_t          size;
} MemEntry;

#define MEM_BLOCKS_CAPACITY 8
typedef struct MemBlocks {
    queue_node_t   node;
    size_t      count;  // Number of entries in use
    MemEntry    blocks[MEM_BLOCKS_CAPACITY];
} MemBlocks;


void AddressSpace_Init(AddressSpaceRef _Nonnull self)
{
    self->mblocks = QUEUE_INIT;
    mtx_init(&self->mtx);
}

void AddressSpace_Deinit(AddressSpaceRef _Nonnull self)
{
    AddressSpace_UnmapAll(self);
    mtx_deinit(&self->mtx);
}

size_t AddressSpace_GetVirtualSize(AddressSpaceRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const size_t vsize = self->virt_size;
    mtx_unlock(&self->mtx);

    return vsize;
}

// Allocates more address space to the calling process. The address space is
// expanded by 'nbytes' bytes. A pointer to the first byte in the newly allocated
// address space portion is return in 'pOutMem'. 'pOutMem' is set to NULL and a
// suitable error is returned if the allocation failed. 'nbytes' must be greater
// than 0 and a multiple of the CPU page size.
errno_t AddressSpace_Allocate(AddressSpaceRef _Nonnull self, ssize_t nbytes, void* _Nullable * _Nonnull pOutMem)
{
    decl_try_err();
    MemBlocks* bp = NULL;
    char* p = NULL;

    if (nbytes == 0) {
        return EINVAL;
    }
    if (__Floor_PowerOf2(nbytes, CPU_PAGE_SIZE) != nbytes) {
        return EINVAL;
    }


    mtx_lock(&self->mtx);

    // We don't need to free the MemBlocks if the allocation of the memory block
    // below fails because we can always keep it around for the next allocation
    // request
    if (queue_empty(&self->mblocks)
        || ((MemBlocks*)self->mblocks.last)->count == MEM_BLOCKS_CAPACITY) {
        try(kalloc_cleared(sizeof(MemBlocks), (void**) &bp));
        bp->node = QUEUE_NODE_INIT;
        queue_add_last(&self->mblocks, &bp->node);
    } else {
        bp = (MemBlocks*)self->mblocks.last;
    }


    // Allocate the memory block
    try(kalloc(nbytes, (void**) &p));


    // Add the memory block to our list
    bp->blocks[bp->count].mem = p;
    bp->blocks[bp->count].size = nbytes;
    bp->count++;
    self->virt_size += nbytes;

catch:
    mtx_unlock(&self->mtx);

    *pOutMem = p;
    return err;
}

static void _AddressSpace_UnmapAll(AddressSpaceRef _Nonnull _Locked self)
{
    MemBlocks* cp = (MemBlocks*)self->mblocks.first;

    while (cp) {
        MemBlocks* np = (MemBlocks*)cp->node.next;

        for (size_t i = 0; i < cp->count; i++) {
            kfree(cp->blocks[i].mem);
            cp->blocks[i].mem = NULL;
            cp->blocks[i].size = 0;
        }

        cp->node = QUEUE_NODE_INIT;
        kfree(cp);

        cp = np;
    }

    self->mblocks = QUEUE_INIT;
    self->virt_size = 0;
}

void AddressSpace_UnmapAll(AddressSpaceRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    _AddressSpace_UnmapAll(self);
    mtx_unlock(&self->mtx);
}

void AddressSpace_AdoptMappingsFrom(AddressSpaceRef _Nonnull self, AddressSpaceRef _Nonnull other)
{
    mtx_lock(&self->mtx);
    _AddressSpace_UnmapAll(self);

    self->mblocks = other->mblocks;
    self->virt_size = other->virt_size;
    other->mblocks = QUEUE_INIT;
    mtx_unlock(&self->mtx);
}
