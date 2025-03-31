//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include "DiskDriverChannel.h"
#include <diskcache/DiskCache.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverChannel.h>


errno_t DiskDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, DiskCacheRef _Nonnull diskCache, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = NULL;

    try(Driver_Create(pClass, kDriver_Exclusive | kDriver_Seekable, parent, (DriverRef*)&self));
    self->diskCache = diskCache;
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

    self->diskCache = NULL;
}

errno_t DiskDriver_createDispatchQueue(DiskDriverRef _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    return DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, pOutQueue);
}

errno_t DiskDriver_onPublish(DiskDriverRef _Nonnull self)
{
    return DiskCache_RegisterDisk(gDiskCache, self);
}

void DiskDriver_onUnpublish(DiskDriverRef _Nonnull self)
{
    DiskCache_UnregisterDisk(gDiskCache, self);
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
        self->blockSize = pInfo->blockSize;
        self->blockCount = pInfo->blockCount;
        self->blockShift = u_ispow2(pInfo->blockSize) ? u_log2(pInfo->blockSize) : 0;
        self->isReadOnly = pInfo->isReadOnly;
    
        self->currentMediaId++;
        while (self->currentMediaId == kMediaId_None || self->currentMediaId == kMediaId_Current) {
            self->currentMediaId++;
        }
    }
    else {
        self->blockCount = 0;
        self->blockSize = 0;
        self->blockShift = 0;
        self->isReadOnly = true;

        self->currentMediaId = kMediaId_None;
    }
    Driver_Unlock(self);
}

typedef errno_t (*phys_block_func_t)(DiskDriverRef _Nonnull self, LogicalBlockAddress ba, uint8_t* _Nonnull data, size_t blockSize);

void DiskDriver_doIO(DiskDriverRef _Nonnull self, DiskRequest* _Nonnull req)
{
    decl_try_err();

    Driver_Lock(self);
    const MediaId curMediaId = self->currentMediaId;
    const size_t logicalBlockSize = DiskCache_GetBlockSize(self->diskCache);
    const size_t physBlockSize = self->blockSize;
    Driver_Unlock(self);

    phys_block_func_t pbf;
    switch (req->type) {
        case kDiskRequest_Read:
            pbf = (phys_block_func_t)implementationof(getBlock, DiskDriver, classof(self));
            break;

        case kDiskRequest_Write:
            pbf = (phys_block_func_t)implementationof(putBlock, DiskDriver, classof(self));
            break;

        default:
            err = EIO;
            break;
    }

    if (req->r.mediaId == kMediaId_Current || req->r.mediaId == curMediaId) {
        for (size_t i = 0; (err == EOK) && (i < req->r.blockCount); i++) {
            err = pbf(self, req->r.lba + i, &req->r.data[i * logicalBlockSize], physBlockSize);
        }
    }
    else {
        err = EDISKCHANGE;
    }

    DiskDriver_EndIO(self, req, err);
}

errno_t DiskDriver_beginIO(DiskDriverRef _Nonnull _Locked self, DiskRequest* _Nonnull req)
{
    if (self->dispatchQueue) {
        return DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)implementationof(doIO, DiskDriver, classof(self)), self, req, 0, 0, 0);
    }
    else {
        invoke_n(doIO, DiskDriver, self, req);
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


errno_t DiskDriver_getBlock(DiskDriverRef _Nonnull self, LogicalBlockAddress ba, uint8_t* _Nonnull data, size_t blockSize)
{
    return EIO;
}

errno_t DiskDriver_putBlock(DiskDriverRef _Nonnull self, LogicalBlockAddress ba, const uint8_t* _Nonnull data, size_t blockSize)
{
    return EIO;
}


void DiskDriver_endIO(DiskDriverRef _Nonnull _Locked self, DiskRequest* _Nonnull req, errno_t status)
{
    DiskRequest_Done(req, status);
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

        errno_t e1 = DiskCache_MapBlock(gDiskCache, self, mediaId, blockIdx, kMapBlock_ReadOnly, &blk);
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        memcpy(dp, blk.data + blockOffset, nBytesToReadInBlock);
        DiskCache_UnmapBlock(gDiskCache, blk.token, kWriteBlock_None);

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

        errno_t e1 = DiskCache_MapBlock(gDiskCache, self, mediaId, blockIdx, mmode, &blk);
        if (e1 == EOK) {
            memcpy(blk.data + blockOffset, sp, nBytesToWriteInBlock);
            e1 = DiskCache_UnmapBlock(gDiskCache, blk.token, kWriteBlock_Sync);
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

off_t DiskDriver_getSeekableRange(DiskDriverRef _Nonnull self)
{
    DiskInfo info;

    DiskDriver_GetInfo(self, &info);
    return info.blockCount * info.blockSize;
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
