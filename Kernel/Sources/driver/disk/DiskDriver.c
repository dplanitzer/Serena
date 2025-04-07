//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include <diskcache/DiskCache.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverChannel.h>


errno_t DiskDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, const MediaInfo* _Nullable info, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = NULL;

    try(Driver_Create(pClass, kDriver_Exclusive | kDriver_Seekable, parent, (DriverRef*)&self));
    self->diskCache = gDiskCache;
    self->currentMediaId = kMediaId_None;

    DiskDriver_NoteMediaLoaded(self, info);

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

    self->diskCache = NULL;
}

errno_t DiskDriver_createDispatchQueue(DiskDriverRef _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    return DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, pOutQueue);
}

errno_t DiskDriver_onPublish(DiskDriverRef _Nonnull self)
{
    return DiskCache_RegisterDisk(self->diskCache, self);
}

void DiskDriver_onUnpublish(DiskDriverRef _Nonnull self)
{
    DiskCache_UnregisterDisk(self->diskCache, self);
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
    pOutInfo->mediaId = self->currentMediaId;
    pOutInfo->isReadOnly = self->isReadOnly;
    pOutInfo->reserved[0] = 0;
    pOutInfo->reserved[1] = 0;
    pOutInfo->reserved[2] = 0;
    pOutInfo->blockSize = self->blockSize;
    pOutInfo->blockCount = self->blockCount;
    pOutInfo->mediaBlockSize = self->mediaBlockSize;
    pOutInfo->mediaBlockCount = self->mediaBlockCount;
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
    const bool hasMedia = (pInfo && (pInfo->blockCount > 0) && (pInfo->blockSize > 0)) ? true : false;

    Driver_Lock(self);

    self->mediaBlockCount = pInfo->blockCount;
    self->mediaBlockSize = pInfo->blockSize;
    self->blockSize = DiskCache_GetBlockSize(self->diskCache);
    self->blockShift = u_log2(self->blockSize);

    if (self->mediaBlockSize > 0 && u_ispow2(self->mediaBlockSize)) {
        self->mb2lbFactor = self->blockSize / self->mediaBlockSize;
    }
    else {
        self->mb2lbFactor = 1;
    }

    self->blockCount = self->mediaBlockCount / self->mb2lbFactor;

    if (hasMedia) {
        self->isReadOnly = pInfo->isReadOnly;
    
        self->currentMediaId++;
        while (self->currentMediaId == kMediaId_None || self->currentMediaId == kMediaId_Current) {
            self->currentMediaId++;
        }
    }
    else {
        self->isReadOnly = true;

        self->currentMediaId = kMediaId_None;
    }

    Driver_Unlock(self);
}

errno_t DiskDriver_doBlockRequest(DiskDriverRef _Nonnull self, const DiskContext* _Nonnull ctx, DiskRequest* _Nonnull req, BlockRequest* _Nonnull br)
{
    decl_try_err();
    const LogicalBlockAddress lba = br->lba;
    uint8_t* data = br->data;
    uint8_t* edata = data + ctx->mBlockSize * ctx->mb2lbFactor;
    LogicalBlockAddress mba = lba * ctx->mb2lbFactor;
    const bool shouldZeroFill = ((req->type == kDiskRequest_Read) && (ctx->lBlockSize > ctx->mBlockSize) && (ctx->mb2lbFactor == 1)) ? true : false;

    if (req->mediaId != ctx->mediaId && req->mediaId != kMediaId_Current) {
        return EDISKCHANGE;
    }
    if (lba >= ctx->lBlockCount) {
        return ENXIO;
    }

    while ((data < edata) && (err == EOK)) {
        switch (req->type) {
            case kDiskRequest_Read:
                err = DiskDriver_GetMediaBlock(self, mba, data, ctx->mBlockSize);
                break;

            case kDiskRequest_Write:
                err = DiskDriver_PutMediaBlock(self, mba, data, ctx->mBlockSize);
                break;

            default:
                err = EIO;
                break;
        }

        data += ctx->mBlockSize;
        mba++;
    }

    if (err == EOK && shouldZeroFill) {
        // Media block size isn't a power-of-2 and this is a read. We zero-fill
        // the remaining bytes in the logical block
        memset(data + ctx->mBlockSize, 0, ctx->lBlockSize - ctx->mBlockSize);
    }

    return err;
}

void DiskDriver_doDiskRequest(DiskDriverRef _Nonnull self, const DiskContext* _Nonnull ctx, DiskRequest* _Nonnull req)
{
    for (size_t i = 0; i < req->rCount; i++) {
        BlockRequest* br = &req->r[i];
        const errno_t err = DiskDriver_DoBlockRequest(self, ctx, req, br);
    
        DiskRequest_Done(req, br, err);
        // Continue with the next block request even if the current one failed
        // with an error. We want to get as many good requests done as possible.
    }
}

static void DiskDriver_DoIO(DiskDriverRef _Nonnull self, DiskRequest* _Nonnull req)
{
    decl_try_err();
    DiskContext ctx;

    Driver_Lock(self);
    ctx.mediaId = self->currentMediaId;
    ctx.lBlockCount = self->blockCount;
    ctx.lBlockSize = self->blockSize;
    ctx.mBlockSize = self->mediaBlockSize;
    ctx.mb2lbFactor = self->mb2lbFactor;
    Driver_Unlock(self);

    DiskDriver_DoDiskRequest(self, &ctx, req);
    DiskRequest_Done(req, NULL, EOK);
}

errno_t DiskDriver_beginIO(DiskDriverRef _Nonnull _Locked self, DiskRequest* _Nonnull req)
{
    if (self->dispatchQueue) {
        return DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)DiskDriver_DoIO, self, req, 0, 0, 0);
    }
    else {
        DiskDriver_DoIO(self, req);
        return EOK;
    }
}

errno_t DiskDriver_BeginIO(DiskDriverRef _Nonnull self, DiskRequest* _Nonnull req)
{
    decl_try_err();

    Driver_Lock(self);
    if (Driver_IsActive(self)) {
        err = invoke_n(beginIO, DiskDriver, self, req);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

    return err;
}


errno_t DiskDriver_getMediaBlock(DiskDriverRef _Nonnull self, LogicalBlockAddress ba, uint8_t* _Nonnull data, size_t mbSize)
{
    return EIO;
}

errno_t DiskDriver_putMediaBlock(DiskDriverRef _Nonnull self, LogicalBlockAddress ba, const uint8_t* _Nonnull data, size_t mbSize)
{
    return EIO;
}


//
// MARK: -
// I/O Channel API
//

off_t DiskDriver_getSeekableRange(DiskDriverRef _Nonnull self)
{
    Driver_Lock(self);
    const off_t r = (off_t)self->blockCount * (off_t)self->blockSize;
    Driver_Unlock(self);

    return r;
}

errno_t DiskDriver_read(DiskDriverRef _Nonnull self, DiskDriverChannelRef _Nonnull ch, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const off_t diskSize = IOChannel_GetSeekableRange(ch);
    off_t offset = IOChannel_GetOffset(ch);
    uint8_t* dp = pBuffer;
    ssize_t nBytesRead = 0;

    if (nBytesToRead < 0) {
        throw(EINVAL);
    }
    if (nBytesToRead > 0) {
        if (offset < 0ll) {
            throw(EOVERFLOW);
        }
        if (offset >= diskSize) {
            throw(ENXIO);
        }
    }


    // Limit 'nBytesToRead' to the range of bytes that is actually available
    // starting at 'offset'.
    const off_t nAvailBytes = diskSize - offset;

    if (nAvailBytes > 0) {
        if (nAvailBytes <= (off_t)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToRead) {
            nBytesToRead = (ssize_t)nAvailBytes;
        }
        // Otherwise, use 'nBytesToRead' as is
    } else {
        nBytesToRead = 0;
    }


    // Get the block index and block offset that corresponds to 'offset'. We start
    // iterating through blocks there.
    Driver_Lock(self);
    const MediaId mediaId = self->currentMediaId;
    const size_t blockSize = self->blockSize;
    const size_t blockShift = self->blockShift;
    Driver_Unlock(self);

    if (blockShift == 0) {
        throw(EIO);
    }

    const size_t blockMask = blockSize - 1;
    LogicalBlockAddress blockIdx = (LogicalBlockAddress)(offset >> (off_t)blockShift);
    ssize_t blockOffset = (ssize_t)(offset & (off_t)blockMask);


    // Iterate through a contiguous sequence of blocks until we've read all
    // required bytes.
    while (nBytesToRead > 0) {
        const ssize_t nRemainderBlockSize = (ssize_t)blockSize - blockOffset;
        const ssize_t nBytesToReadInBlock = (nBytesToRead > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToRead;
        FSBlock blk = {0};

        errno_t e1 = DiskCache_MapBlock(self->diskCache, self, mediaId, blockIdx, kMapBlock_ReadOnly, &blk);
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        memcpy(dp, blk.data + blockOffset, nBytesToReadInBlock);
        DiskCache_UnmapBlock(self->diskCache, blk.token, kWriteBlock_None);

        nBytesToRead -= nBytesToReadInBlock;
        nBytesRead += nBytesToReadInBlock;
        dp += nBytesToReadInBlock;

        blockOffset = 0;
        blockIdx++;
    }

    if (nBytesRead > 0) {
        IOChannel_IncrementOffsetBy(ch, nBytesRead);
    }


catch:
    *pOutBytesRead = nBytesRead;
    return err;
}

errno_t DiskDriver_write(DiskDriverRef _Nonnull self, DiskDriverChannelRef _Nonnull ch, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    const off_t diskSize = IOChannel_GetSeekableRange(ch);
    off_t offset = IOChannel_GetOffset(ch);
    const uint8_t* sp = buf;
    ssize_t nBytesWritten = 0;

    if (nBytesToWrite < 0) {
        throw(EINVAL);
    }
    if (nBytesToWrite > 0) {
        if (offset < 0ll) {
            throw(EOVERFLOW);
        }
        if (offset >= diskSize) {
            throw(ENXIO);
        }
    }


    // Limit 'nBytesToWrite' to the range of bytes that is actually available
    // starting at 'offset'.
    const off_t nAvailBytes = diskSize - offset;

    if (nAvailBytes > 0) {
        if (nAvailBytes <= (off_t)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToWrite) {
            nBytesToWrite = (ssize_t)nAvailBytes;
        }
        // Otherwise, use 'nBytesToWrite' as is
    } else {
        nBytesToWrite = 0;
    }


    // Get the block index and block offset that corresponds to 'offset'. We start
    // iterating through blocks there.
    Driver_Lock(self);
    const MediaId mediaId = self->currentMediaId;
    const size_t blockSize = self->blockSize;
    const size_t blockShift = self->blockShift;
    Driver_Unlock(self);

    if (blockShift == 0) {
        throw(EIO);
    }
    
    const size_t blockMask = blockSize - 1;
    LogicalBlockAddress blockIdx = (LogicalBlockAddress)(offset >> (off_t)blockShift);
    ssize_t blockOffset = (ssize_t)(offset & (off_t)blockMask);


    // Iterate through a contiguous sequence of blocks until we've written all
    // required bytes.
    while (nBytesToWrite > 0) {
        const ssize_t nRemainderBlockSize = (ssize_t)blockSize - blockOffset;
        const ssize_t nBytesToWriteInBlock = (nBytesToWrite > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToWrite;
        MapBlock mmode = (nBytesToWriteInBlock == (ssize_t)blockSize) ? kMapBlock_Replace : kMapBlock_Update;
        FSBlock blk = {0};

        errno_t e1 = DiskCache_MapBlock(self->diskCache, self, mediaId, blockIdx, mmode, &blk);
        if (e1 == EOK) {
            memcpy(blk.data + blockOffset, sp, nBytesToWriteInBlock);
            e1 = DiskCache_UnmapBlock(self->diskCache, blk.token, kWriteBlock_Sync);
        }
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }

        nBytesToWrite -= nBytesToWriteInBlock;
        nBytesWritten += nBytesToWriteInBlock;
        sp += nBytesToWriteInBlock;

        blockOffset = 0;
        blockIdx++;
    }

    if (nBytesWritten > 0) {
        IOChannel_IncrementOffsetBy(ch, nBytesWritten);
    }


catch:
    *pOutBytesWritten = nBytesWritten;
    return err;
}

errno_t DiskDriver_ioctl(DiskDriverRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kDiskCommand_GetInfo:
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
func_def(doDiskRequest, DiskDriver)
func_def(doBlockRequest, DiskDriver)
func_def(getMediaBlock, DiskDriver)
func_def(putMediaBlock, DiskDriver)
override_func_def(getSeekableRange, DiskDriver, Driver)
override_func_def(read, DiskDriver, Driver)
override_func_def(write, DiskDriver, Driver)
override_func_def(ioctl, DiskDriver, Driver)
);
