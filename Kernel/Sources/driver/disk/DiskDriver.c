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

struct GetInfoReq {
    DiskInfo* _Nonnull  info;       // out
    errno_t             err;        // out
};


// Generates a new unique media ID. Call this function to generate a new media
// ID after a media change has been detected and use the returned value as the
// new current media ID.
// Note: must be called from the disk driver dispatch queue.
MediaId DiskDriver_GetNewMediaId(DiskDriverRef _Nonnull self)
{
    self->nextMediaId++;
    while (self->nextMediaId == kMediaId_None || self->nextMediaId == kMediaId_Current) {
        self->nextMediaId++;
    }

    return self->nextMediaId;
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

static void DiskDriver_getInfoStub(DiskDriverRef _Nonnull self, struct GetInfoReq* _Nonnull rq)
{
    rq->err = invoke_n(getInfo_async, DiskDriver, self, rq->info);
}

errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    struct GetInfoReq rq;

    rq.info = pOutInfo;
    rq.err = EOK;

    DispatchQueue_DispatchClosure(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_getInfoStub, self, &rq, 0, kDispatchOption_Sync, 0);
    return rq.err;
}

// Returns information about the disk drive and the media loaded into the
// drive.
errno_t DiskDriver_getInfo_async(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    pOutInfo->diskId = DiskDriver_GetDiskId(self);
    pOutInfo->mediaId = DiskDriver_GetCurrentMediaId(self);
    pOutInfo->isReadOnly = true;
    pOutInfo->reserved[0] = 0;
    pOutInfo->reserved[1] = 0;
    pOutInfo->reserved[2] = 0;
    pOutInfo->blockSize = 512;
    pOutInfo->blockCount = 0;

    return EOK;
}


MediaId DiskDriver_getCurrentMediaId(DiskDriverRef _Nonnull self)
{
    return kMediaId_None;
}


static void DiskDriver_beginIOStub(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    invoke_n(beginIO_async, DiskDriver, self, pBlock);
}

errno_t DiskDriver_BeginIO(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    return DispatchQueue_DispatchClosure(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_beginIOStub, self, pBlock, 0, 0, 0);
}

void DiskDriver_beginIO_async(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    decl_try_err();
    const MediaId physMediaId = DiskBlock_GetPhysicalAddress(pBlock)->mediaId;

    if (physMediaId == kMediaId_Current
        || physMediaId == DiskDriver_GetCurrentMediaId(self)) {
        switch (DiskBlock_GetOp(pBlock)) {
            case kDiskBlockOp_Read:
                err = DiskDriver_GetBlock(self, pBlock);
                break;

            case kDiskBlockOp_Write:
                err = DiskDriver_PutBlock(self, pBlock);
                break;

            default:
                err = EIO;
                break;
        }
    }
    else {
        err = EDISKCHANGE;
    }

    DiskDriver_EndIO(self, pBlock, err);
}


// Reads the contents of the given block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
// The abstract implementation returns EIO.
errno_t DiskDriver_getBlock(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    return EIO;
}


// Writes the contents of 'pBlock' to the corresponding disk block. Blocks
// the caller until the write has completed. The contents of the block on
// disk is left in an indeterminate state of the write fails in the middle
// of the write. The block may contain a mix of old and new data.
// The abstract implementation returns EIO.
errno_t DiskDriver_putBlock(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    return EIO;
}


void DiskDriver_endIO(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock, errno_t status)
{
    DiskCache_OnBlockFinishedIO(gDiskCache, self, pBlock, status);
}


//
// MARK: -
// I/O Channel API
//

errno_t DiskDriver_createChannel(DiskDriverRef _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    DiskInfo info;

    err = DiskDriver_GetInfo(self, &info);
    if (err == EOK) {
        err = DiskDriverChannel_Create(self, &info, mode, pOutChannel);
    }
    return err;
}

errno_t DiskDriver_read(DiskDriverRef _Nonnull self, DiskDriverChannelRef _Nonnull pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const DiskInfo* info = DiskDriverChannel_GetInfo(pChannel);
    const FileOffset diskSize = DriverChannel_GetSeekableRange(pChannel);
    FileOffset offset = DriverChannel_GetOffset(pChannel);
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
    const FileOffset diskSize = DriverChannel_GetSeekableRange(pChannel);
    FileOffset offset = DriverChannel_GetOffset(pChannel);
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
override_func_def(onPublish, DiskDriver, Driver)
override_func_def(onUnpublish, DiskDriver, Driver)
func_def(getInfo_async, DiskDriver)
func_def(getCurrentMediaId, DiskDriver)
func_def(beginIO_async, DiskDriver)
func_def(getBlock, DiskDriver)
func_def(putBlock, DiskDriver)
func_def(endIO, DiskDriver)
override_func_def(createChannel, DiskDriver, Driver)
override_func_def(read, DiskDriver, Driver)
override_func_def(write, DiskDriver, Driver)
override_func_def(getSeekableRange, DiskDriver, Driver)
override_func_def(ioctl, DiskDriver, Driver)
);
