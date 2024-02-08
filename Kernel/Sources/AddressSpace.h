//
//  AddressSpace.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/20/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef AddressSpace_h
#define AddressSpace_h

#include <klib/klib.h>


struct _AddressSpace;
typedef struct _AddressSpace* AddressSpaceRef;


extern errno_t AddressSpace_Create(AddressSpaceRef _Nullable * _Nonnull pOutSpace);
extern void AddressSpace_Destroy(AddressSpaceRef _Nullable pSpace);

extern bool AddressSpace_IsEmpty(AddressSpaceRef _Nonnull pSpace);

extern errno_t AddressSpace_Allocate(AddressSpaceRef _Nonnull pSpace, ssize_t count, Byte* _Nullable * _Nonnull pOutMem);

#endif /* AddressSpace_h */
