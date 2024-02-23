//
//  Process_Descriptors.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"


// Registers the given resource in the given resource table. This action allows
// the process to use this resource and it will keep the resource alive until
// it is unregistered or the process exits. The process maintains a strong
// reference to the resource until it is unregistered. Note that the process
// retains the resource and thus you have to release it once the call returns.
// The call returns a descriptor which can be used to refer to the resource from
// user and/or kernel space.
static errno_t Process_RegisterResource_Locked(ProcessRef _Nonnull self, ObjectRef _Nonnull pResource, ObjectArrayRef _Nonnull pArray, int* _Nonnull pOutDescriptor)
{
    decl_try_err();

    // Find the lowest descriptor id that is available
    ssize_t desc = ObjectArray_GetCount(pArray);
    bool hasFoundSlot = false;
    for (ssize_t i = 0; i < desc; i++) {
        if (ObjectArray_GetAt(pArray, i) == NULL) {
            desc = i;
            hasFoundSlot = true;
            break;
        }
    }


    // Expand the descriptor table if we didn't find an empty slot
    if (hasFoundSlot) {
        ObjectArray_ReplaceAt(pArray, pResource, desc);
    } else {
        err = ObjectArray_Add(pArray, pResource);
    }

    *pOutDescriptor = (err == EOK) ? (int)desc : -1;
    return err;
}

// Unregisters the resource identified by the given descriptor. The resource is
// removed from the given resource table and a strong reference to the resource
// is returned. The caller should call Object_Release() to release the strong
// reference to the resource.
static errno_t Process_UnregisterResource(ProcessRef _Nonnull self, int desc, ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable * _Nonnull pOutResource)
{
    decl_try_err();
    ObjectRef pResource = NULL;

    Lock_Lock(&self->lock);

    if (desc >= 0 && desc < ObjectArray_GetCount(pArray)) {
        pResource = ObjectArray_ExtractOwnershipAt(pArray, desc);
    }
    
    Lock_Unlock(&self->lock);

    *pOutResource = pResource;
    return (pResource) ? EOK : EBADF;
}

// Looks up the resource identified by the given descriptor and returns a strong
// reference to it if found. The caller should call Object_Release() on the
// resource once it is no longer needed.
static errno_t Process_CopyResourceForDescriptor(ProcessRef _Nonnull self, int desc, ObjectArrayRef _Nonnull pArray, ObjectRef _Nullable * _Nonnull pOutResource)
{
    decl_try_err();
    ObjectRef pResource = NULL;

    Lock_Lock(&self->lock);

    if (desc >= 0 && desc < ObjectArray_GetCount(pArray)
        && (pResource = ObjectArray_GetAt(pArray, desc)) != NULL) {
        Object_Retain(pResource);
    } else {
        err = EBADF;
    }

    Lock_Unlock(&self->lock);

    *pOutResource = pResource;
    return err;
}


////////////////////////////////////////////////////////////////////////////////

// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
errno_t Process_RegisterIOChannel_Locked(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor)
{
    return Process_RegisterResource_Locked(self, (ObjectRef) pChannel, &self->ioChannels, pOutDescriptor);
}

// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
errno_t Process_RegisterIOChannel(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor)
{
    Lock_Lock(&self->lock);
    const errno_t err = Process_RegisterIOChannel_Locked(self, pChannel, pOutDescriptor);
    Lock_Unlock(&self->lock);

    return err;
}

// Unregisters the I/O channel identified by the given descriptor. The channel
// is removed from the process' I/O channel table and a strong reference to the
// channel is returned. The caller should call close() on the channel to close
// it and then release() to release the strong reference to the channel. Closing
// the channel will mark itself as done and the channel will be deallocated once
// the last strong reference to it has been released.
errno_t Process_UnregisterIOChannel(ProcessRef _Nonnull self, int ioc, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return Process_UnregisterResource(self, ioc, &self->ioChannels, (ObjectRef*) pOutChannel);
}

// Closes all registered I/O channels. Ignores any errors that may be returned
// from the close() call of a channel.
void Process_CloseAllIOChannels_Locked(ProcessRef _Nonnull self)
{
    for (ssize_t ioc = 0; ioc < ObjectArray_GetCount(&self->ioChannels); ioc++) {
        IOChannelRef pChannel = (IOChannelRef) ObjectArray_GetAt(&self->ioChannels, ioc);

        if (pChannel) {
            IOChannel_Close(pChannel);
        }
    }
}

// Looks up the I/O channel identified by the given descriptor and returns a
// strong reference to it if found. The caller should call release() on the
// channel once it is no longer needed.
errno_t Process_CopyIOChannelForDescriptor(ProcessRef _Nonnull self, int ioc, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return Process_CopyResourceForDescriptor(self, ioc, &self->ioChannels, (ObjectRef*) pOutChannel);
}


////////////////////////////////////////////////////////////////////////////////

// Registers the given private resource with the process. This action allows the
// process to use this private resource. The process maintains a strong reference
// to the private resource until it is unregistered. Note that the process retains
// the private resource and thus you have to release it once the call returns.
// The call returns a descriptor which can be used to refer to the private
// resource from user and/or kernel space.
errno_t Process_RegisterPrivateResource_Locked(ProcessRef _Nonnull self, ObjectRef _Nonnull pResource, int* _Nonnull pOutDescriptor)
{
    return Process_RegisterResource_Locked(self, pResource, &self->privateResources, pOutDescriptor);
}

// Disposes off all registered private resources.
void Process_DisposeAllPrivateResources_Locked(ProcessRef _Nonnull self)
{
    for (ssize_t desc = 0; desc < ObjectArray_GetCount(&self->privateResources); desc++) {
        Object_Release(ObjectArray_GetAt(&self->privateResources, desc));
    }
}

// Looks up the private resource identified by the given descriptor and returns
// a strong reference to it if found. The caller should call release() on the
// private resource once it is no longer needed.
errno_t Process_CopyPrivateResourceForDescriptor(ProcessRef _Nonnull self, int od, ObjectRef _Nullable * _Nonnull pOutResource)
{
    return Process_CopyResourceForDescriptor(self, od, &self->privateResources, (ObjectRef*) pOutResource);
}
