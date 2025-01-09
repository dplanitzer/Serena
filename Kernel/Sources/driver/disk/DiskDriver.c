//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include "DiskDriverChannel.h"
#include <disk/DiskCache.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverChannel.h>


errno_t _DiskDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = NULL;

    try(_Driver_Create(pClass, kDriver_Exclusive | kDriver_Seekable, (DriverRef*)&self));
    self->diskId = kDiskId_None;
    self->currentMediaId = kMediaId_None;

    if ((options & kDiskDriver_Queuing) == kDiskDriver_Queuing) {
        try(DiskDriver_CreateDispatchQueue(self, &self->dispatchQueue));
    }

    *pOutSelf = (DriverRef)self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void DiskDriver_deinit(DiskDriverRef _Nonnull self)
{
    if (self->dispatchQueue) {
        Object_Release(self->dispatchQueue);
        self->dispatchQueue = NULL;
    }
}

errno_t DiskDriver_createDispatchQueue(DiskDriverRef _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    return DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, pOutQueue);
}

errno_t DiskDriver_onPublish(DiskDriverRef _Nonnull self)
{
    return DiskCache_RegisterDisk(gDiskCache, self, &self->diskId);
}

void DiskDriver_onUnpublish(DiskDriverRef _Nonnull self)
{
    if (self->diskId != kDiskId_None) {
        DiskCache_UnregisterDisk(gDiskCache, self->diskId);
        self->diskId = kDiskId_None;
    }
}

void DiskDriver_onStop(DiskDriverRef _Nonnull _Locked self)
{
    if (self->dispatchQueue) {
        DispatchQueue_Terminate(self->dispatchQueue);
        DispatchQueue_WaitForTerminationCompleted(self->dispatchQueue);
    }
}


// Returns information about the disk drive and the media loaded into the
// drive.
void DiskDriver_getInfo(DiskDriverRef _Nonnull _Locked self, DiskInfo* _Nonnull pOutInfo)
{
    pOutInfo->diskId = self->diskId;
    pOutInfo->mediaId = self->currentMediaId;
    pOutInfo->isReadOnly = self->mediaInfo.isReadOnly;
    pOutInfo->reserved[0] = 0;
    pOutInfo->reserved[1] = 0;
    pOutInfo->reserved[2] = 0;
    pOutInfo->blockSize = self->mediaInfo.blockSize;
    pOutInfo->blockCount = self->mediaInfo.blockCount;
}

errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    decl_try_err();

    Driver_Lock(self);
    if (Driver_IsActive(self)) {
        invoke_n(getInfo, DiskDriver, self, pOutInfo);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

    return err;
}

void DiskDriver_NoteMediaLoaded(DiskDriverRef _Nonnull self, const MediaInfo* _Nullable pInfo)
{
    Driver_Lock(self);
    if (pInfo) {
        self->mediaInfo = *pInfo;

        self->currentMediaId++;
        while (self->currentMediaId == kMediaId_None || self->currentMediaId == kMediaId_Current) {
            self->currentMediaId++;
        }
    }
    else {
        self->mediaInfo.blockCount = 0;
        self->mediaInfo.blockSize = 0;
        self->mediaInfo.isReadOnly = true;

        self->currentMediaId = kMediaId_None;
    }
    Driver_Unlock(self);
}


MediaId DiskDriver_getCurrentMediaId(DiskDriverRef _Nonnull self)
{
    return kMediaId_None;
}


void DiskDriver_doIO(DiskDriverRef _Nonnull self, const IORequest* _Nonnull ior)
{
    decl_try_err();

    Driver_Lock(self);
    const MediaId curMediaId = self->currentMediaId;
    Driver_Unlock(self);

    if (ior->address.mediaId == kMediaId_Current || ior->address.mediaId == curMediaId) {
        switch (DiskBlock_GetOp(ior->block)) {
            case kDiskBlockOp_Read:
                err = DiskDriver_GetBlock(self, ior);
                break;

            case kDiskBlockOp_Write:
                err = DiskDriver_PutBlock(self, ior);
                break;

            default:
                err = EIO;
                break;
        }
    }
    else {
        err = EDISKCHANGE;
    }

    DiskDriver_EndIO(self, ior->block, err);
}

errno_t DiskDriver_beginIO(DiskDriverRef _Nonnull _Locked self, const IORequest* _Nonnull ior)
{
    if (self->dispatchQueue) {
        return DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)implementationof(doIO, DiskDriver, classof(self)), self, ior, sizeof(IORequest), 0, 0);
    }
    else {
        invoke_n(doIO, DiskDriver, self, ior);
        return EOK;
    }
}

errno_t DiskDriver_BeginIO(DiskDriverRef _Nonnull self, const IORequest* _Nonnull ior)
{
    decl_try_err();

    Driver_Lock(self);
    if (Driver_IsActive(self)) {
        err = invoke_n(beginIO, DiskDriver, self, ior);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

    return err;
}


// Reads the contents of the given block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
// The abstract implementation returns EIO.
errno_t DiskDriver_getBlock(DiskDriverRef _Nonnull self, const IORequest* _Nonnull ior)
{
    return EIO;
}


// Writes the contents of 'pBlock' to the corresponding disk block. Blocks
// the caller until the write has completed. The contents of the block on
// disk is left in an indeterminate state of the write fails in the middle
// of the write. The block may contain a mix of old and new data.
// The abstract implementation returns EIO.
errno_t DiskDriver_putBlock(DiskDriverRef _Nonnull self, const IORequest* _Nonnull ior)
{
    return EIO;
}


void DiskDriver_endIO(DiskDriverRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, errno_t status)
{
    DiskCache_OnBlockFinishedIO(gDiskCache, self, pBlock, status);
}


//
// MARK: -
// I/O Channel API
//

errno_t DiskDriver_createChannel(DiskDriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    DiskInfo info;

    invoke_n(getInfo, DiskDriver, self, &info);
    return DiskDriverChannel_Create(self, &info, mode, pOutChannel);
}

errno_t DiskDriver_read(DiskDriverRef _Nonnull self, DiskDriverChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const DiskInfo* info = DiskDriverChannel_GetInfo(pChannel);
    const FileOffset diskSize = IOChannel_GetSeekableRange(pChannel);
    FileOffset offset = IOChannel_GetOffset(pChannel);
    uint8_t* dp = pBuffer;
    ssize_t nBytesRead = 0;

    if (nBytesToRead > 0) {
        if (offset < 0ll || offset >= diskSize) {
            *pOutBytesRead = 0;
            return EOVERFLOW;
        }

        const FileOffset targetOffset = offset + (FileOffset)nBytesToRead;
        if (targetOffset < 0ll || targetOffset > diskSize) {
            nBytesToRead = (ssize_t)(diskSize - offset);
        }
    }
    else if (nBytesToRead < 0) {
        return EINVAL;
    }

    while (nBytesToRead > 0 && offset < diskSize) {
        const int blockIdx = (int)(offset / (FileOffset)info->blockSize);    //XXX blockIdx should be 64bit
        const size_t blockOffset = offset % (FileOffset)info->blockSize;     //XXX optimize for power-of-2
        const size_t nBytesToReadInCurrentBlock = (size_t)__min((FileOffset)(info->blockSize - blockOffset), __min(diskSize - offset, (FileOffset)nBytesToRead));
        DiskBlockRef pBlock;

        errno_t e1 = DiskCache_AcquireBlock(gDiskCache, info->diskId, info->mediaId, blockIdx, kAcquireBlock_ReadOnly, &pBlock);
        if (e1 == EOK) {
            const uint8_t* bp = DiskBlock_GetData(pBlock);
            
            memcpy(dp, bp + blockOffset, nBytesToReadInCurrentBlock);
            DiskCache_RelinquishBlock(gDiskCache, pBlock);
        }
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }

        nBytesToRead -= nBytesToReadInCurrentBlock;
        nBytesRead += nBytesToReadInCurrentBlock;
        offset += (FileOffset)nBytesToReadInCurrentBlock;
        dp += nBytesToReadInCurrentBlock;
    }

    *pOutBytesRead = nBytesRead;

    return err;
}

errno_t DiskDriver_write(DiskDriverRef _Nonnull self, DiskDriverChannelRef _Nonnull pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    const DiskInfo* info = DiskDriverChannel_GetInfo(pChannel);
    const FileOffset diskSize = IOChannel_GetSeekableRange(pChannel);
    FileOffset offset = IOChannel_GetOffset(pChannel);
    ssize_t nBytesWritten = 0;

    if (nBytesToWrite > 0) {
        if (offset < 0ll || offset >= diskSize) {
            *pOutBytesWritten = 0;
            return EOVERFLOW;
        }

        const FileOffset targetOffset = offset + (FileOffset)nBytesToWrite;
        if (targetOffset < 0ll || targetOffset > diskSize) {
            nBytesToWrite = (ssize_t)(diskSize - offset);
        }
    }
    else if (nBytesToWrite < 0) {
        return EINVAL;
    }

    while (nBytesToWrite > 0) {
        const int blockIdx = (int)(offset / (FileOffset)info->blockSize);    //XXX blockIdx should be 64bit
        const size_t blockOffset = offset % (FileOffset)info->blockSize;     //XXX optimize for power-of-2
        const size_t nBytesToWriteInCurrentBlock = __min(info->blockSize - blockOffset, nBytesToWrite);
        AcquireBlock acquireMode = (nBytesToWriteInCurrentBlock == info->blockSize) ? kAcquireBlock_Replace : kAcquireBlock_Update;
        DiskBlockRef pBlock;

        errno_t e1 = DiskCache_AcquireBlock(gDiskCache, info->diskId, info->mediaId, blockIdx, acquireMode, &pBlock);
        if (e1 == EOK) {
            uint8_t* bp = DiskBlock_GetMutableData(pBlock);
        
            memcpy(bp + blockOffset, ((const uint8_t*) pBuffer) + nBytesWritten, nBytesToWriteInCurrentBlock);
            e1 = DiskCache_RelinquishBlockWriting(gDiskCache, pBlock, kWriteBlock_Sync);
        }
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }

        nBytesToWrite -= nBytesToWriteInCurrentBlock;
        nBytesWritten += nBytesToWriteInCurrentBlock;
        offset += (FileOffset)nBytesToWriteInCurrentBlock;
    }

    *pOutBytesWritten = nBytesWritten;
    return err;
}

FileOffset DiskDriver_getSeekableRange(DiskDriverRef _Nonnull self)
{
    DiskInfo info;

    DiskDriver_GetInfo(self, &info);
    return info.blockCount * info.blockSize;
}

errno_t DiskDriver_ioctl(DiskDriverRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIODiskCommand_GetInfo:
            return DiskDriver_GetInfo(self, va_arg(ap, DiskInfo*));
            
        default:
            return super_n(ioctl, Driver, DiskDriver, self, cmd, ap);
    }
}


class_func_defs(DiskDriver, Driver,
override_func_def(deinit, DiskDriver, Object)
func_def(createDispatchQueue, DiskDriver)
override_func_def(onPublish, DiskDriver, Driver)
override_func_def(onUnpublish, DiskDriver, Driver)
override_func_def(onStop, DiskDriver, Driver)
func_def(getInfo, DiskDriver)
func_def(beginIO, DiskDriver)
func_def(doIO, DiskDriver)
func_def(getBlock, DiskDriver)
func_def(putBlock, DiskDriver)
func_def(endIO, DiskDriver)
override_func_def(createChannel, DiskDriver, Driver)
override_func_def(read, DiskDriver, Driver)
override_func_def(write, DiskDriver, Driver)
override_func_def(getSeekableRange, DiskDriver, Driver)
override_func_def(ioctl, DiskDriver, Driver)
);
