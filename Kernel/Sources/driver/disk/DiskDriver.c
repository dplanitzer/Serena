//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverChannel.h>
#include <log/Log.h>


errno_t DiskDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, const MediaInfo* _Nullable info, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = NULL;

    try(Driver_Create(pClass, kDriver_Exclusive | kDriver_Seekable, parent, (DriverRef*)&self));
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
}

errno_t DiskDriver_createDispatchQueue(DiskDriverRef _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue)
{
    return DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, pOutQueue);
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
    pOutInfo->sectorSize = self->sectorSize;
    pOutInfo->sectorCount = self->sectorCount;
    pOutInfo->rwClusterSize = self->rwClusterSize;
    pOutInfo->frClusterSize = self->frClusterSize;
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

    self->rwClusterSize = info->rwClusterSize;
    self->frClusterSize = info->frClusterSize;
    self->sectorCount = sectorCount;
    self->sectorSize = info->sectorSize;

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
    if (self->flags.isChsLinear) {
        chs->c = 1;
        chs->h = 1;
        chs->s = lsa;
    }
    else {
        chs->c = lsa / (sno_t)self->sectorsPerCylinder;
        chs->h = (lsa / (sno_t)self->sectorsPerTrack) % (sno_t)self->headsPerCylinder;
        chs->s = lsa % (sno_t)self->sectorsPerTrack;
    }
}

sno_t DiskDriver_ChsToLsa(DiskDriverRef _Locked _Nonnull self, const chs_t* _Nonnull chs)
{
    if (self->flags.isChsLinear) {
        return chs->s;
    }
    else {
        return (chs->c * (sno_t)self->headsPerCylinder + chs->h) * (sno_t)self->sectorsPerTrack + chs->s;
    }
}


errno_t DiskDriver_getSector(DiskDriverRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t mbSize)
{
    return EIO;
}

errno_t DiskDriver_putSector(DiskDriverRef _Nonnull self, const chs_t* _Nonnull chs, const uint8_t* _Nonnull data, size_t mbSize)
{
    return EIO;
}

void DiskDriver_sectorStrategy(DiskDriverRef _Nonnull self, DiskRequest* _Nonnull req)
{
    decl_try_err();
    chs_t chs;
    ssize_t resCount = 0;
    sno_t lsa = req->offset / self->sectorSize;

    for (size_t i = 0; (i < req->iovCount) && (err == EOK); i++) {
        IOVector* iov = &req->iov[i];
        ssize_t size = iov->size;
        uint8_t* data = iov->data;

        if (size < 0) {
            err = EINVAL;
            break;
        }
        else if (size > 0 && req->offset < 0ll) {
            err = EOVERFLOW;
            break;
        }
        
        while (size >= self->sectorSize) {
            DiskDriver_LsaToChs(self, lsa, &chs);

            if (req->mediaId != self->currentMediaId) {
                err = EDISKCHANGE;
            }        
            else if (lsa >= self->sectorCount) {
                err = ENXIO;
            }
            else if (req->s.type == kDiskRequest_Read) {
                err = DiskDriver_GetSector(self, &chs, data, self->sectorSize);
            }
            else if (req->s.type == kDiskRequest_Write) {
                err = DiskDriver_PutSector(self, &chs, data, self->sectorSize);
            }
            else {
                err = EIO;
            }

            if (err != EOK) {
                break;
            }
    

            data += self->sectorSize;
            size -= self->sectorSize;
            resCount += self->sectorSize;
            lsa++;
        }
    }

    req->resCount = resCount;
    req->s.status = (resCount > 0) ? EOK : err;
}

void DiskDriver_strategy(DiskDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    Driver_Lock(self);
    if (!Driver_IsActive(self)) {
        req->status = ENODEV;
    }
    Driver_Unlock(self);


    if (req->status == EOK) {
        DiskDriver_SectorStrategy(self, (DiskRequest*)req);
    }

    IORequest_Done(req);
}


errno_t DiskDriver_beginIO(DiskDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    return DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)implementationof(strategy, DiskDriver, classof(self)), self, req, 0, 0, 0);
}

errno_t DiskDriver_doIO(DiskDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    errno_t err = DispatchQueue_DispatchClosure(self->dispatchQueue, (VoidFunc_2)implementationof(strategy, DiskDriver, classof(self)), self, req, 0, kDispatchOption_Sync, 0);
    if (err == EOK) {
        err = req->status;
    }
    return err;
}


errno_t DiskDriver_formatSectors(DiskDriverRef _Nonnull self, const chs_t* chs, const void* _Nonnull data, size_t secSize)
{
    return ENOTSUP;
}

errno_t DiskDriver_doFormat(DiskDriverRef _Nonnull _Locked self, FormatSectorsRequest* _Nonnull req)
{
    chs_t chs;

    DiskDriver_LsaToChs(self, req->addr, &chs);
    return DiskDriver_FormatSectors(self, &chs, req->data, self->sectorSize);
}

void _DiskDriver_xDoFormat(DiskDriverRef _Nonnull _Locked self, FormatSectorsRequest* _Nonnull req)
{
    decl_try_err();

    if (req->mediaId != self->currentMediaId) {
        req->status = EDISKCHANGE;
    }
    else if (req->addr + self->frClusterSize > self->sectorCount) {
        req->status = ENXIO;
    }
    else {
        req->status = DiskDriver_DoFormat(self, req);
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
    const off_t r = (off_t)self->sectorCount * (off_t)self->sectorSize;
    Driver_Unlock(self);

    return r;
}

static errno_t _DiskDriver_rdwr(DiskDriverRef _Nonnull self, int type, DiskDriverChannelRef _Nonnull ch, void* _Nonnull buf, ssize_t byteCount, ssize_t* _Nonnull pOutByteCount)
{
    decl_try_err();
    DiskRequest req;

    req.s.type = type;
    req.s.size = sizeof(DiskRequest);
    req.s.status = EOK;
    req.iovCount = 1;
    req.mediaId = self->currentMediaId;
    req.offset = IOChannel_GetOffset(ch);
    req.iov[0].data = buf;
    req.iov[0].size = byteCount;
    req.resCount = 0;

    err = DiskDriver_DoIO(self, (IORequest*)&req);

    if (req.resCount > 0) {
        IOChannel_IncrementOffsetBy(ch, req.resCount);
    }

    *pOutByteCount = req.resCount;
    return err;
}

errno_t DiskDriver_read(DiskDriverRef _Nonnull self, DiskDriverChannelRef _Nonnull ch, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    return _DiskDriver_rdwr(self, kDiskRequest_Read, ch, buf, nBytesToRead, pOutBytesRead);
}

errno_t DiskDriver_write(DiskDriverRef _Nonnull self, DiskDriverChannelRef _Nonnull ch, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    return _DiskDriver_rdwr(self, kDiskRequest_Write, ch, buf, nBytesToWrite, pOutBytesWritten);
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
override_func_def(onStop, DiskDriver, Driver)
func_def(getInfo, DiskDriver)
func_def(beginIO, DiskDriver)
func_def(doIO, DiskDriver)
func_def(strategy, DiskDriver)
func_def(sectorStrategy, DiskDriver)
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
