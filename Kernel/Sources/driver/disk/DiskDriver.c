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
#include <log/Log.h>


errno_t DiskDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, const MediaInfo* _Nullable info, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = NULL;

    try(Driver_Create(pClass, kDriver_Exclusive | kDriver_Seekable, parent, (DriverRef*)&self));
    self->diskCache = gDiskCache;
    self->currentMediaId = kMediaId_None;

    DiskDriver_NoteMediaLoaded(self, info);
    try(DiskDriver_CreateDispatchQueue(self, &self->dispatchQueue));

    *pOutSelf = (DriverRef)self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void DiskDriver_deinit(DiskDriverRef _Nonnull self)
{
    Object_Release(self->dispatchQueue);
    self->dispatchQueue = NULL;

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
    pOutInfo->properties = self->mediaProperties;
    pOutInfo->blockSize = self->blockSize;
    pOutInfo->blockCount = self->blockCount;
    pOutInfo->sectorSize = self->sectorSize;
    pOutInfo->sectorCount = self->sectorCount;
    pOutInfo->formatSectorCount = self->formatSectorCount;
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

void DiskDriver_NoteMediaLoaded(DiskDriverRef _Nonnull self, const MediaInfo* _Nullable info)
{
    const scnt_t sectorCount = (info) ? (scnt_t)info->sectorsPerTrack * (scnt_t)info->heads * (scnt_t)info->cylinders : 0;
    const bool hasMedia = (info && (sectorCount > 0) && (info->sectorSize > 0)) ? true : false;

    Driver_Lock(self);

    self->sectorsPerTrack = info->sectorsPerTrack;
    self->headsPerCylinder = info->heads;
    self->cylindersPerDisk = info->cylinders;
    self->sectorsPerCylinder = self->headsPerCylinder * self->sectorsPerTrack;
    self->flags.isChsLinear = (info->heads == 1 && info->cylinders == 1);

    self->formatSectorCount = info->formatSectorCount;
    self->sectorCount = sectorCount;
    self->sectorSize = info->sectorSize;
    self->blockSize = DiskCache_GetBlockSize(self->diskCache);
    self->blockShift = u_log2(self->blockSize);

    if (self->sectorSize > 0 && u_ispow2(self->sectorSize)) {
        self->s2bFactor = self->blockSize / self->sectorSize;
    }
    else {
        self->s2bFactor = 1;
    }

    self->blockCount = self->sectorCount / self->s2bFactor;

    if (hasMedia) {
        self->mediaProperties = info->properties;
    
        self->currentMediaId++;
        while (self->currentMediaId == kMediaId_None) {
            self->currentMediaId++;
        }
    }
    else {
        self->mediaProperties = kMediaProperty_IsReadOnly;
        self->currentMediaId = kMediaId_None;
    }

    Driver_Unlock(self);
}

void DiskDriver_LsaToChs(DiskDriverRef _Locked _Nonnull self, sno_t lsa, chs_t* _Nonnull chs)
{
    chs->c = lsa / (sno_t)self->sectorsPerCylinder;
    chs->h = (lsa / (sno_t)self->sectorsPerTrack) % (sno_t)self->headsPerCylinder;
    chs->s = lsa % (sno_t)self->sectorsPerTrack;    
}

sno_t DiskDriver_ChsToLsa(DiskDriverRef _Locked _Nonnull self, const chs_t* _Nonnull chs)
{
    return (chs->c * (sno_t)self->headsPerCylinder + chs->h) * (sno_t)self->sectorsPerTrack + chs->s;
}

errno_t DiskDriver_GetRequestRange(DiskDriverRef _Nonnull self, MediaId mediaId, LogicalBlockAddress lba, brng_t* _Nonnull pOutBlockRange)
{
    decl_try_err();

    Driver_Lock(self);
    if (mediaId == self->currentMediaId) {
        err = DiskDriver_GetRequestRange2(self, mediaId, lba * self->s2bFactor, pOutBlockRange);
        pOutBlockRange->lba = pOutBlockRange->lba / self->s2bFactor;
    }
    else {
        err = EDISKCHANGE;
    }
    Driver_Unlock(self);

    //printf("lba: %d -> [%d, %d)\n", (int)lba, (int)pOutBlockRange->lba, (int)(pOutBlockRange->lba + pOutBlockRange->count));
    return err;
}

errno_t DiskDriver_getRequestRange2(DiskDriverRef _Nonnull self, MediaId mediaId, LogicalBlockAddress lba, brng_t* _Nonnull pOutBlockRange)
{
    pOutBlockRange->lba = lba;
    pOutBlockRange->count = 1;
    return EOK;
}


errno_t DiskDriver_getSector(DiskDriverRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t mbSize)
{
    return EIO;
}

errno_t DiskDriver_putSector(DiskDriverRef _Nonnull self, const chs_t* _Nonnull chs, const uint8_t* _Nonnull data, size_t mbSize)
{
    return EIO;
}

errno_t DiskDriver_doBlockIO(DiskDriverRef _Nonnull self, const DiskContext* _Nonnull ctx, DiskRequest* _Nonnull req, BlockRequest* _Nonnull br)
{
    decl_try_err();
    const LogicalBlockAddress lba = br->lba;
    uint8_t* data = br->data;
    uint8_t* edata = data + ctx->sectorSize * ctx->s2bFactor;
    LogicalBlockAddress lsa = lba * ctx->s2bFactor;
    chs_t chs;
    const bool shouldZeroFill = ((req->type == kDiskRequest_Read) && (ctx->blockSize > ctx->sectorSize) && (ctx->s2bFactor == 1)) ? true : false;

    if (req->mediaId != ctx->mediaId) {
        return EDISKCHANGE;
    }
    if (lba >= ctx->blockCount) {
        return ENXIO;
    }

    while ((data < edata) && (err == EOK)) {
        DiskDriver_LsaToChs(self, lsa, &chs);

        switch (req->type) {
            case kDiskRequest_Read:
                err = DiskDriver_GetSector(self, &chs, data, ctx->sectorSize);
                break;

            case kDiskRequest_Write:
                err = DiskDriver_PutSector(self, &chs, data, ctx->sectorSize);
                break;

            default:
                err = EIO;
                break;
        }

        data += ctx->sectorSize;
        lsa++;
    }

    if (err == EOK && shouldZeroFill) {
        // Media block size isn't a power-of-2 and this is a read. We zero-fill
        // the remaining bytes in the logical block
        memset(data + ctx->sectorSize, 0, ctx->blockSize - ctx->sectorSize);
    }

    return err;
}

void DiskDriver_doIO(DiskDriverRef _Nonnull self, const DiskContext* _Nonnull ctx, DiskRequest* _Nonnull req)
{
    for (size_t i = 0; i < req->rCount; i++) {
        BlockRequest* br = &req->r[i];
        const errno_t err = DiskDriver_DoBlockIO(self, ctx, req, br);
    
        DiskRequest_Done(req, br, err);
        // Continue with the next block request even if the current one failed
        // with an error. We want to get as many good requests done as possible.
    }
}

static void _DiskDriver_xDoIO(DiskDriverRef _Nonnull self, DiskRequest* _Nonnull req)
{
    decl_try_err();
    DiskContext ctx;

    Driver_Lock(self);
    ctx.mediaId = self->currentMediaId;
    ctx.blockCount = self->blockCount;
    ctx.blockSize = self->blockSize;
    ctx.sectorSize = self->sectorSize;
    ctx.s2bFactor = self->s2bFactor;
    Driver_Unlock(self);

    DiskDriver_DoIO(self, &ctx, req);
    DiskRequest_Done(req, NULL, EOK);
}

errno_t DiskDriver_beginIO(DiskDriverRef _Nonnull _Locked self, DiskRequest* _Nonnull req)
{
    return DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)_DiskDriver_xDoIO, self, req, 0, 0, 0);
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


errno_t DiskDriver_formatSectors(DiskDriverRef _Nonnull self, const chs_t* chs, const void* _Nonnull data, size_t secSize)
{
    return ENOTSUP;
}

errno_t DiskDriver_doFormat(DiskDriverRef _Nonnull _Locked self, const DiskContext* _Nonnull ctx, FormatSectorsRequest* _Nonnull req)
{
    chs_t chs;

    DiskDriver_LsaToChs(self, req->addr, &chs);
    return DiskDriver_FormatSectors(self, &chs, req->data, ctx->sectorSize);
}

void _DiskDriver_xDoFormat(DiskDriverRef _Nonnull _Locked self, FormatSectorsRequest* _Nonnull req)
{
    decl_try_err();
    DiskContext ctx;

    // DiskDriver_Format() is holding the lock
    ctx.mediaId = self->currentMediaId;
    ctx.blockCount = self->blockCount;
    ctx.blockSize = self->blockSize;
    ctx.sectorSize = self->sectorSize;
    ctx.s2bFactor = self->s2bFactor;

    if (req->mediaId != ctx.mediaId) {
        req->status = EDISKCHANGE;
    }
    else if (req->addr + self->formatSectorCount > self->sectorCount) {
        req->status = ENXIO;
    }
    else {
        req->status = DiskDriver_DoFormat(self, &ctx, req);
    }
}

errno_t DiskDriver_format(DiskDriverRef _Nonnull _Locked self, FormatSectorsRequest* _Nonnull req)
{
    req->status = EOK;
    errno_t err = DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)_DiskDriver_xDoFormat, self, req, 0, kDispatchOption_Sync, 0);
    if (err == EOK) {
        err = req->status;
    }
    return err;
}

errno_t DiskDriver_Format(DiskDriverRef _Nonnull self, FormatSectorsRequest* _Nonnull req)
{
    decl_try_err();

    Driver_Lock(self);
    if (Driver_IsActive(self)) {
        err = invoke_n(format, DiskDriver, self, req);
    }
    else {
        err = ENODEV;
    }
    Driver_Unlock(self);

    return err;
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
    DiskSession s;


    // Iterate through a contiguous sequence of blocks until we've read all
    // required bytes.
    DiskCache_OpenSession(self->diskCache, (IOChannelRef)ch, mediaId, &s);
    while (nBytesToRead > 0) {
        const ssize_t nRemainderBlockSize = (ssize_t)blockSize - blockOffset;
        const ssize_t nBytesToReadInBlock = (nBytesToRead > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToRead;
        FSBlock blk = {0};

        errno_t e1 = DiskCache_MapBlock(self->diskCache, &s, blockIdx, kMapBlock_ReadOnly, &blk);
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        memcpy(dp, blk.data + blockOffset, nBytesToReadInBlock);
        DiskCache_UnmapBlock(self->diskCache, &s, blk.token, kWriteBlock_None);

        nBytesToRead -= nBytesToReadInBlock;
        nBytesRead += nBytesToReadInBlock;
        dp += nBytesToReadInBlock;

        blockOffset = 0;
        blockIdx++;
    }
    DiskCache_CloseSession(self->diskCache, &s);

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
    DiskSession s;


    // Iterate through a contiguous sequence of blocks until we've written all
    // required bytes.
    DiskCache_OpenSession(self->diskCache, (IOChannelRef)ch, mediaId, &s);
    while (nBytesToWrite > 0) {
        const ssize_t nRemainderBlockSize = (ssize_t)blockSize - blockOffset;
        const ssize_t nBytesToWriteInBlock = (nBytesToWrite > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToWrite;
        MapBlock mmode = (nBytesToWriteInBlock == (ssize_t)blockSize) ? kMapBlock_Replace : kMapBlock_Update;
        FSBlock blk = {0};

        errno_t e1 = DiskCache_MapBlock(self->diskCache, &s, blockIdx, mmode, &blk);
        if (e1 == EOK) {
            memcpy(blk.data + blockOffset, sp, nBytesToWriteInBlock);
            e1 = DiskCache_UnmapBlock(self->diskCache, &s, blk.token, kWriteBlock_Sync);
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
    DiskCache_CloseSession(self->diskCache, &s);

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
        case kDiskCommand_GetInfo: {
            DiskInfo* info = va_arg(ap, DiskInfo*);
            
            return DiskDriver_GetInfo(self, info);
        }

        case kDiskCommand_Format: {
            FormatSectorsRequest* req = va_arg(ap, FormatSectorsRequest*);
            
            return DiskDriver_Format(self, req);
        }

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
func_def(getRequestRange2, DiskDriver)
func_def(beginIO, DiskDriver)
func_def(doIO, DiskDriver)
func_def(doBlockIO, DiskDriver)
func_def(getSector, DiskDriver)
func_def(putSector, DiskDriver)
func_def(format, DiskDriver)
func_def(doFormat, DiskDriver)
func_def(formatSectors, DiskDriver)
override_func_def(getSeekableRange, DiskDriver, Driver)
override_func_def(read, DiskDriver, Driver)
override_func_def(write, DiskDriver, Driver)
override_func_def(ioctl, DiskDriver, Driver)
);
