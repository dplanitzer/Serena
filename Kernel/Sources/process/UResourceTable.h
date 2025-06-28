//
//  UResourceTable.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef UResourceTable_h
#define UResourceTable_h

#include <dispatcher/Lock.h>
#include <UResource.h>


typedef struct UResourceTable {
    UResourceRef _Nullable * _Nonnull   table;
    size_t                              tableCapacity;
    size_t                              resourcesCount;
    Lock                                lock;
} UResourceTable;


extern errno_t UResourceTable_Init(UResourceTable* _Nonnull self);
extern void UResourceTable_Deinit(UResourceTable* _Nonnull self);

// Finds an empty slot in the resource table and stores the resource there.
// Returns the resource descriptor and EOK on success and a suitable error and
// -1 otherwise. Note that this function takes ownership of the provided
// resource.
extern errno_t UResourceTable_AdoptResource(UResourceTable* _Nonnull self, UResourceRef _Consuming _Nonnull pRes, int * _Nonnull pOutDescriptor);

// Disposes the resource at the index 'desc'. Disposing a resource means that
// the entry (name/descriptor) 'desc' is removed from the table and that the
// resource is scheduled for deallocation and deallocated as soon as all still
// ongoing operations have completed.
extern errno_t UResourceTable_DisposeResource(UResourceTable* _Nonnull self, int desc);

// Returns the resource that is named by 'desc'. The resource is guaranteed to
// stay alive until it is relinquished. You should relinquish the resource by
// calling UResourceTable_RelinquishResource(). Returns the resource and EOK on
// success and a suitable error and NULL otherwise.
extern errno_t UResourceTable_AcquireResource(UResourceTable* _Nonnull self, int desc, Class* _Nonnull pExpectedClass, UResourceRef _Nullable * _Nonnull pOutResource);

#define UResourceTable_AcquireResourceAs(__self, __desc, __className, __pOutResource) \
UResourceTable_AcquireResource(__self, __desc, &k##__className##Class, (UResourceRef*)__pOutResource)

// Relinquishes the given resource. The resource must have been acquired previously
// by calling UResourceTable_AcquireResource(). Note that the resource may be
// freed by this function. It is not safe to continue to use the resource reference
// once this function returns.
#define UResourceTable_RelinquishResource(__self, __pResource) \
UResource_EndOperation(__pResource)


// Begins direct access on the resource identified by the descriptor 'desc'.
// The resource is expected to be an instance of class 'pExpectedClass'. Returns
// a reference to the resource on success and an error otherwise.
// Note that this function leaves the resource table locked on success. You must
// call the end-direct-resource-access method once you're done with the resource.
// Direct resource access should only be used for cases where the resource
// operation is running for a very short amount of time and can not block for a
// potentially long time.
// Note that the resource is guaranteed to stay alive while direct access is
// active. It can not be destroyed until direct access ends.
// Note that only one execution thread at a time can directly access a resource
// since the resource table stays locked while direct access is ongoing. Thus,
// again, no long running operations should be executed on the resource. Use the
// acquisition and relinquish model documented above for long running resource
// operations.
//
// The usage pattern is:
// 
// try(UResourceTable_BeginDirectResourceAccess(pTable, ..., pRes));
// Resource_Do(pRes);
// UResourceTable_EndDirectResourceAccess(pTable);
extern errno_t UResourceTable_BeginDirectResourceAccess(UResourceTable* _Nonnull self, int desc, Class* _Nonnull pExpectedClass, UResourceRef _Nullable * _Nonnull pOutResource);

#define UResourceTable_BeginDirectResourceAccessAs(__self, __desc, __className, __pOutResource) \
UResourceTable_BeginDirectResourceAccess(__self, __desc, &k##__className##Class, (UResourceRef*)__pOutResource)

// Ends direct access to a resource and unlocks the resource table.
extern void UResourceTable_EndDirectResourceAccess(UResourceTable* _Nonnull self);

#endif /* UResourceTable_h */
