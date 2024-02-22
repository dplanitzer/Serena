//
//  Process_Descriptors.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"


// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
errno_t Process_RegisterIOChannel_Locked(ProcessRef _Nonnull pProc, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor)
{
    decl_try_err();

    // Find the lowest descriptor id that is available
    ssize_t ioc = ObjectArray_GetCount(&pProc->ioChannels);
    bool hasFoundSlot = false;
    for (ssize_t i = 0; i < ioc; i++) {
        if (ObjectArray_GetAt(&pProc->ioChannels, i) == NULL) {
            ioc = i;
            hasFoundSlot = true;
            break;
        }
    }


    // Expand the descriptor table if we didn't find an empty slot
    if (hasFoundSlot) {
        ObjectArray_ReplaceAt(&pProc->ioChannels, (ObjectRef) pChannel, ioc);
    } else {
        err = ObjectArray_Add(&pProc->ioChannels, (ObjectRef) pChannel);
    }

    *pOutDescriptor = (err == EOK) ? (int)ioc : -1;
    return err;
}

// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
errno_t Process_RegisterIOChannel(ProcessRef _Nonnull pProc, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = Process_RegisterIOChannel_Locked(pProc, pChannel, pOutDescriptor);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Unregisters the I/O channel identified by the given descriptor. The channel
// is removed from the process' I/O channel table and a strong reference to the
// channel is returned. The caller should call close() on the channel to close
// it and then release() to release the strong reference to the channel. Closing
// the channel will mark itself as done and the channel will be deallocated once
// the last strong reference to it has been released.
errno_t Process_UnregisterIOChannel(ProcessRef _Nonnull pProc, int ioc, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel = NULL;

    Lock_Lock(&pProc->lock);

    if (ioc >= 0 && ioc < ObjectArray_GetCount(&pProc->ioChannels)) {
        pChannel = (IOChannelRef) ObjectArray_ExtractOwnershipAt(&pProc->ioChannels, ioc);
    }
    
    Lock_Unlock(&pProc->lock);

    *pOutChannel = pChannel;
    return (pChannel) ? EOK : EBADF;
}

// Closes all registered I/O channels. Ignores any errors that may be returned
// from the close() call of a channel.
void Process_CloseAllIOChannels_Locked(ProcessRef _Nonnull pProc)
{
    for (ssize_t ioc = 0; ioc < ObjectArray_GetCount(&pProc->ioChannels); ioc++) {
        IOChannelRef pChannel = (IOChannelRef) ObjectArray_GetAt(&pProc->ioChannels, ioc);

        if (pChannel) {
            IOChannel_Close(pChannel);
        }
    }
}

// Looks up the I/O channel identified by the given descriptor and returns a
// strong reference to it if found. The caller should call release() on the
// channel once it is no longer needed.
errno_t Process_CopyIOChannelForDescriptor(ProcessRef _Nonnull pProc, int ioc, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel = NULL;

    Lock_Lock(&pProc->lock);

    if (ioc >= 0 && ioc < ObjectArray_GetCount(&pProc->ioChannels)
        && (pChannel = (IOChannelRef) ObjectArray_GetAt(&pProc->ioChannels, ioc)) != NULL) {
        Object_Retain(pChannel);
    } else {
        err = EBADF;
    }

    Lock_Unlock(&pProc->lock);

    *pOutChannel = pChannel;
    return err;
}
