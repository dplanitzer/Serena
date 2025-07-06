//
//  UResourceTable.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "UResourceTable.h"
#include <kern/kalloc.h>
#include <kern/limits.h>


enum {
    kUResourceTable_PageSize = 64
};


errno_t UResourceTable_Init(UResourceTable* _Nonnull self)
{
    decl_try_err();

    try(kalloc_cleared(sizeof(UResourceRef) * kUResourceTable_PageSize, (void**)&self->table));
    self->tableCapacity = kUResourceTable_PageSize;
    Lock_Init(&self->lock);

catch:
    return err;
}

void UResourceTable_Deinit(UResourceTable* _Nonnull self)
{
    UResourceRef* table;
    size_t resCount;

    Lock_Lock(&self->lock);
    table = self->table;
    resCount = self->resourcesCount;
    self->table = NULL;
    self->tableCapacity = 0;
    self->resourcesCount = 0;
    Lock_Unlock(&self->lock);

    for (size_t i = 0, rc = 0; rc < resCount; i++) {
        if (table[i]) {
            UResource_Dispose(table[i]);
            table[i] = NULL;
            rc++;
        }
    }
    kfree(table);

    Lock_Deinit(&self->lock);
}

// Finds an empty slot in the resource table and stores the resource there.
// Returns the resource descriptor and EOK on success and a suitable error and
// -1 otherwise. Note that this function takes ownership of the provided
// resource.
errno_t UResourceTable_AdoptResource(UResourceTable* _Nonnull self, UResourceRef _Consuming _Nonnull pResource, int * _Nonnull pOutDescriptor)
{
    decl_try_err();
    size_t desc = 0;
    bool hasSlot = false;

    Lock_Lock(&self->lock);

    for (size_t i = 0; i < self->tableCapacity; i++) {
        if (self->table[i] == NULL) {
            desc = i;
            hasSlot = true;
            break;
        }
    }

    if (!hasSlot || desc > INT_MAX) {
        throw(EMFILE);
    }

    self->table[desc] = pResource;
    self->resourcesCount++;
    Lock_Unlock(&self->lock);

    *pOutDescriptor = desc;
    return EOK;


catch:
    Lock_Unlock(&self->lock);
    *pOutDescriptor = -1;
    return err;
}

// Disposes the resource at the index 'desc'. Disposing a resource means that
// the entry (name/descriptor) 'desc' is removed from the table and that the
// resource is scheduled for deallocation and deallocated as soon as all still
// ongoing operations have completed.
errno_t UResourceTable_DisposeResource(UResourceTable* _Nonnull self, int desc)
{
    decl_try_err();
    UResourceRef pRes;

    // Do the actual channel release outside the table lock because the release
    // may take some time to execute. Ie it's synchronously draining some buffered
    // data.
    Lock_Lock(&self->lock);
    if (desc < 0 || desc > self->tableCapacity || self->table[desc] == NULL) {
        throw(EBADF);
    }

    pRes = self->table[desc];
    self->table[desc] = NULL;
    self->resourcesCount--;
    Lock_Unlock(&self->lock);

    UResource_Dispose(pRes);
    return EOK;

catch:
    Lock_Unlock(&self->lock);
    return err;
}

// Begins direct access on the resource identified by the descriptor 'desc'.
// The resource is expected to be an instance of class 'pExpectedClass'. Returns
// a reference to the resource on success and an error otherwise.
// Note that this function leaves the resource table locked on success. You must
// call the end-direct-resource-access method once you're done with the resource.
// Direct resource access should only be used for cases where the resource
// operation is running for a very short amount of time and can not block for a
// potentially long time.
errno_t UResourceTable_BeginDirectResourceAccess(UResourceTable* _Nonnull self, int desc, Class* _Nonnull pExpectedClass, UResourceRef _Nullable * _Nonnull pOutResource)
{
    decl_try_err();
    UResourceRef pRes = NULL;

    Lock_Lock(&self->lock);
    if (desc < 0 || desc > self->tableCapacity || self->table[desc] == NULL) {
        throw(EBADF);
    }

    pRes = self->table[desc];
    if (classof(pRes) != pExpectedClass) {
        throw(EBADF);
    }
    *pOutResource = pRes;
    return EOK;

catch:
    Lock_Unlock(&self->lock);
    *pOutResource = NULL;
    return err;
}

// Ends direct access to a resource and unlocks the resource table.
void UResourceTable_EndDirectResourceAccess(UResourceTable* _Nonnull self)
{
    Lock_Unlock(&self->lock);
}

// Returns the resource that is named by 'desc'. The resource is guaranteed to
// stay alive until it is relinquished. You should relinquish the resource by
// calling UResourceTable_RelinquishResource(). Returns the resource and EOK on
// success and a suitable error and NULL otherwise.
errno_t UResourceTable_AcquireResource(UResourceTable* _Nonnull self, int desc, Class* _Nonnull pExpectedClass, UResourceRef _Nullable * _Nonnull pOutResource)
{
    decl_try_err();
    UResourceRef pRes = NULL;

    Lock_Lock(&self->lock);
    if (desc < 0 || desc > self->tableCapacity || self->table[desc] == NULL) {
        throw(EBADF);
    }

    pRes = self->table[desc];
    if (classof(pRes) != pExpectedClass) {
        throw(EBADF);
    }

    UResource_BeginOperation(pRes);

catch:
    Lock_Unlock(&self->lock);
    *pOutResource = pRes;
    return err;
}

// Returns the resource that is named by 'desc'. The resource is guaranteed to
// stay alive until it is relinquished. You should relinquish the resource by
// calling UResourceTable_RelinquishResource(). Returns the resource and EOK on
// success and a suitable error and NULL otherwise.
errno_t UResourceTable_AcquireTwoResources(UResourceTable* _Nonnull self,
    int desc1, Class* _Nonnull pExpectedClass1, UResourceRef _Nullable * _Nonnull pOutResource1,
    int desc2, Class* _Nonnull pExpectedClass2, UResourceRef _Nullable * _Nonnull pOutResource2)
{
    decl_try_err();
    UResourceRef pRes1 = NULL;
    UResourceRef pRes2 = NULL;

    Lock_Lock(&self->lock);
    if (desc1 < 0 || desc1 > self->tableCapacity || self->table[desc1] == NULL) {
        throw(EBADF);
    }
    if (desc2 < 0 || desc2 > self->tableCapacity || self->table[desc2] == NULL) {
        throw(EBADF);
    }

    pRes1 = self->table[desc1];
    if (classof(pRes1) != pExpectedClass1) {
        throw(EBADF);
    }
    pRes2 = self->table[desc2];
    if (classof(pRes2) != pExpectedClass2) {
        throw(EBADF);
    }

    UResource_BeginOperation(pRes1);
    UResource_BeginOperation(pRes2);

catch:
    Lock_Unlock(&self->lock);
    *pOutResource1 = pRes1;
    *pOutResource2 = pRes2;

    return err;
}