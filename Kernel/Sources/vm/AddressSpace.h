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
#include <kobj/AnyRefs.h>


extern errno_t AddressSpace_Create(AddressSpaceRef _Nullable * _Nonnull pOutSelf);
extern void AddressSpace_Destroy(AddressSpaceRef _Nullable self);

extern void AddressSpace_UnmapAll(AddressSpaceRef _Nonnull self);

extern bool AddressSpace_IsEmpty(AddressSpaceRef _Nonnull self);
extern size_t AddressSpace_GetVirtualSize(AddressSpaceRef _Nonnull self);

extern errno_t AddressSpace_Allocate(AddressSpaceRef _Nonnull self, ssize_t nbytes, void* _Nullable * _Nonnull pOutMem);

#endif /* AddressSpace_h */
