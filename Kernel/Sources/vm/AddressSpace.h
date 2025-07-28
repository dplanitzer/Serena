//
//  AddressSpace.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef AddressSpace_h
#define AddressSpace_h

#include <kern/errno.h>
#include <klib/List.h>
#include <kobj/AnyRefs.h>
#include <sched/mtx.h>


typedef struct AddressSpace {
    SList/*<MemBlocks>*/    mblocks;
    mtx_t                   mtx;
} AddressSpace;


extern void AddressSpace_Init(AddressSpaceRef _Nonnull self);
extern void AddressSpace_Deinit(AddressSpaceRef _Nonnull self);

extern bool AddressSpace_IsEmpty(AddressSpaceRef _Nonnull self);
extern size_t AddressSpace_GetVirtualSize(AddressSpaceRef _Nonnull self);

// Allocates a memory block of size 'nbytes' and adds it to the address space.
extern errno_t AddressSpace_Allocate(AddressSpaceRef _Nonnull self, ssize_t nbytes, void* _Nullable * _Nonnull pOutMem);

// Removes and frees all mappings from the address space. The result is a
// completely empty address space that owns no memory.
extern void AddressSpace_UnmapAll(AddressSpaceRef _Nonnull self);

// Atomically removes and frees all mappings from the address space and then
// adopts all mappings of the address space 'other'. 'self' becomes the owner of
// all mappings previously owned by 'other' and 'other' is left as an empty
// address space.
extern void AddressSpace_AdoptMappingsFrom(AddressSpaceRef _Nonnull self, AddressSpaceRef _Nonnull other);

#endif /* AddressSpace_h */
