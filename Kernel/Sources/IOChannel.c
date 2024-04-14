//
//  IOChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "IOChannel.h"


// Creates an instance of an I/O channel. Subclassers should call this method in
// their own constructor implementation and then initialize the subclass specific
// properties. 
errno_t IOChannel_AbstractCreate(Class* _Nonnull pClass, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef self;

    try(kalloc_cleared(pClass->instanceSize, (void**) &self));
    self->super.clazz = pClass;
    self->retainCount = 1;
    self->mode = mode & (kOpen_ReadWrite | kOpen_Append);

catch:
    *pOutChannel = self;
    return err;
}

// Creates a copy of the given I/O channel. Subclassers should call this in their
// own copying implementation and then copy the subclass specific properties.
errno_t IOChannel_AbstractCreateCopy(IOChannelRef _Nonnull pInChannel, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    IOChannelRef pChannel;

    try(kalloc_cleared(classof(pInChannel)->instanceSize, (void**) &pChannel));
    pChannel->super.clazz = classof(pInChannel);
    pChannel->retainCount = 1;
    pChannel->mode = pInChannel->mode;

catch:
    *pOutChannel = pChannel;
    return err;
}

// Releases a strong reference on the given resource. Deallocates the resource
// when the reference count transitions from 1 to 0. Invokes the deinit method
// on the resource if the resource should be deallocated.
void _IOChannel_Release(IOChannelRef _Nullable self)
{
    if (self == NULL) {
        return;
    }

    const AtomicInt rc = AtomicInt_Decrement(&self->retainCount);

    // Note that we trigger the deallocation when the reference count transitions
    // from 1 to 0. The VP that caused this transition is the one that executes
    // the deallocation code. If another VP calls Release() while we are
    // deallocating the resource then nothing will happen. Most importantly no
    // second deallocation will be triggered. The reference count simply becomes
    // negative which is fine. In that sense a negative reference count signals
    // that the object is dead.
    if (rc == 0) {
        ((IOChannelMethodTable*)self->super.clazz->vtable)->deinit(self);
        kfree(self);
    }
}

void IOChannel_deinit(IOChannelRef _Nonnull self)
{
}

errno_t IOChannel_close(IOChannelRef _Nonnull self)
{
    return EOK;
}

errno_t IOChannel_dup(IOChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return EBADF;
}

errno_t IOChannel_ioctl(IOChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetMode:
            *((unsigned int*) va_arg(ap, unsigned int*)) = self->mode;
            return EOK;

        default:
            return ENOTIOCTLCMD;
    }
}

errno_t IOChannel_read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return EBADF;
}

errno_t IOChannel_Read(IOChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    if ((self->mode & kOpen_Read) == kOpen_Read) {
        return invoke_n(read, IOChannel, self, pBuffer, nBytesToRead, nOutBytesRead);
    } else {
        *nOutBytesRead = 0;
        return EBADF;
    }
}

errno_t IOChannel_write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    return EBADF;
}

errno_t IOChannel_Write(IOChannelRef _Nonnull self, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    if ((self->mode & kOpen_Write) == kOpen_Write) {
        return invoke_n(write, IOChannel, self, pBuffer, nBytesToWrite, nOutBytesWritten);
    } else {
        *nOutBytesWritten = 0;
        return EBADF;
    }
}

errno_t IOChannel_seek(IOChannelRef _Nonnull self, FileOffset offset, FileOffset* pOutPosition, int whence)
{
    *pOutPosition = 0;
    return ESPIPE;
}


any_subclass_func_defs(IOChannel,
func_def(deinit, IOChannel)
func_def(dup, IOChannel)
func_def(ioctl, IOChannel)
func_def(read, IOChannel)
func_def(write, IOChannel)
func_def(seek, IOChannel)
func_def(close, IOChannel)
);
