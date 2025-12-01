//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include <kpi/fcntl.h>


errno_t DiskDriver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, const drive_info_t* _Nonnull driveInfo, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = NULL;

    try(Driver_Create(pClass, kDriver_Exclusive | kDriver_Seekable, cats, (DriverRef*)&self));
    try(DiskDriver_CreateDispatchQueue(self, &self->dq));
    self->driveInfo = *driveInfo;

    // A disk driver always starts out in "no disk in drive" and "disk change is
    // pending" state until reset() triggered by onStart() figures out what the
    // truth really is. Whether there's no disk in the drive or there is one and
    // what the geometry of that disk is. Note that this is the case even for
    // rigid/fixed platter disk drives.
    DiskDriver_NoteSensedDisk(self, NULL);
    self->flags.isDiskChangeActive = 1;

    *pOutSelf = (DriverRef)self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void DiskDriver_deinit(DiskDriverRef _Nonnull self)
{
    if (self->dq) {
        dispatch_destroy(self->dq);
        self->dq = NULL;
    }
}

errno_t DiskDriver_createDispatchQueue(DiskDriverRef _Nonnull self, dispatch_t _Nullable * _Nonnull pOutQueue)
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_URGENT(DISPATCH_PRI_NORMAL);

    return dispatch_create(&attr, pOutQueue);
}

void DiskDriver_onStop(DiskDriverRef _Nonnull _Locked self)
{
    if (self->dq) {
        dispatch_terminate(self->dq, DISPATCH_TERMINATE_AWAIT_ALL);
    }
}


void DiskDriver_NoteSensedDisk(DiskDriverRef _Nonnull self, const SensedDisk* _Nullable info)
{
    if (info) {
        self->sectorsPerTrack = info->sectorsPerTrack;
        self->headsPerCylinder = info->heads;
        self->cylindersPerDisk = info->cylinders;
        self->sectorsPerCylinder = info->heads * info->sectorsPerTrack;
        self->flags.isChsLinear = (info->heads == 1 && info->cylinders == 1);
    
        self->sectorsPerRdwr = info->sectorsPerRdwr;
        self->sectorCount = (scnt_t)info->sectorsPerTrack * (scnt_t)info->heads * (scnt_t)info->cylinders;
        self->sectorSize = info->sectorSize;
    
        self->diskProperties = info->properties;
        self->flags.hasDisk = 1;
        self->flags.isDiskChangeActive = 0;

        self->diskId++;
        if (self->diskId == 0) {
            // wrap around
            self->diskId++;
        }
    }
    else {
        self->sectorsPerTrack = 0;
        self->headsPerCylinder = 1;
        self->cylindersPerDisk = 1;
        self->sectorsPerCylinder = 0;
        self->flags.isChsLinear = 1;
    
        self->sectorsPerRdwr = 1;
        self->sectorCount = 0;
        self->sectorSize = 0;
    
        self->diskProperties = kDisk_IsReadOnly;
        self->flags.hasDisk = 0;
        self->flags.isDiskChangeActive = 0;
    }
}

void DiskDriver_doSenseDisk(DiskDriverRef _Nonnull self, SenseDiskRequest* _Nonnull req)
{

}

errno_t DiskDriver_SenseDisk(DiskDriverRef _Nonnull self)
{
    SenseDiskRequest r;

    IORequest_Init(&r, kDiskRequest_SenseDisk);
    return DiskDriver_DoIO(self, (IORequest*)&r);
}

void DiskDriver_NoteDiskChanged(DiskDriverRef _Nonnull self)
{
    self->flags.isDiskChangeActive = 1;
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

static void _read_write_async(DiskDriverRef _Nonnull self, StrategyRequest* _Nonnull req)
{
    decl_try_err();
    chs_t chs;
    ssize_t resCount = 0;
    sno_t lsa = req->offset / (off_t)self->sectorSize;

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
        else if (self->flags.isDiskChangeActive) {
            err = EDISKCHANGE;
            break;
        }
        else if (!self->flags.hasDisk) {
            err = ENOMEDIUM;
            break;
        }
        
        while (size >= self->sectorSize) {
            DiskDriver_LsaToChs(self, lsa, &chs);

            if (lsa >= self->sectorCount) {
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


errno_t DiskDriver_doFormatDisk(DiskDriverRef _Nonnull self, char fillByte)
{
    return ENOTSUP;
}

errno_t DiskDriver_doFormatTrack(DiskDriverRef _Nonnull self, const chs_t* chs, char fillByte, size_t secSize)
{
    return ENOTSUP;
}

static void _format_track_async(DiskDriverRef _Nonnull self, FormatTrackRequest* _Nonnull req)
{
    decl_try_err();
    const sno_t lsa = req->offset / (off_t)self->sectorSize;
    chs_t chs;

    if (lsa + self->sectorsPerTrack > self->sectorCount) {
        err = ENXIO;
    }
    else if (self->flags.isDiskChangeActive) {
        err = EDISKCHANGE;
    }
        else if (!self->flags.hasDisk) {
        err = ENOMEDIUM;
    }
    else {
        DiskDriver_LsaToChs(self, lsa, &chs);
        err = DiskDriver_DoFormatTrack(self, &chs, req->fillByte, self->sectorSize);
    }

    if (err == EOK) {
        req->resCount = self->sectorsPerTrack * self->sectorSize;
    }
    req->s.status = err;
}


// Returns information about the disk drive.
static void _get_drive_info_async(DiskDriverRef _Nonnull self, GetDriveInfoRequest* _Nonnull req)
{
    drive_info_t* p = req->ip;
    
    p->properties = self->diskProperties;
    req->s.status = EOK;
}

static void _get_disk_info_async(DiskDriverRef _Nonnull self, DiskGeometryRequest* _Nonnull req)
{
    disk_info_t* p = req->gp;

    if (self->flags.isDiskChangeActive) {
        req->s.status = EDISKCHANGE;
    }
    else if (self->flags.hasDisk) {
        p->heads = self->headsPerCylinder;
        p->cylinders = self->cylindersPerDisk;
        p->sectorsPerTrack = self->sectorsPerTrack;
        p->sectorsPerDisk = self->sectorCount;
        p->sectorSize = self->sectorSize;
        p->sectorsPerRdwr = self->sectorsPerRdwr;
        p->diskId = self->diskId;
        p->properties = self->diskProperties;
        req->s.status = EOK;
    }
    else {
        req->s.status = ENOMEDIUM;
    }
}


void DiskDriver_handleRequest(DiskDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    if (!Driver_IsActive(self)) {
        req->status = ENODEV;
    }


    if (req->status == EOK) {
        switch (req->type) {
            case kDiskRequest_Read:
            case kDiskRequest_Write:
                _read_write_async(self, (StrategyRequest*)req);
                break;

            case kDiskRequest_FormatDisk:
                req->status = DiskDriver_DoFormatDisk(self, ((FormatDiskRequest*)req)->fillByte);
                break;

            case kDiskRequest_FormatTrack:
                _format_track_async(self, (FormatTrackRequest*)req);
                break;

            case kDiskRequest_GetDriveInfo:
                _get_drive_info_async(self, (GetDriveInfoRequest*)req);
                break;

            case kDiskRequest_GetDiskInfo:
                _get_disk_info_async(self, (DiskGeometryRequest*)req);
                break;

            case kDiskRequest_SenseDisk:
                DiskDriver_DoSenseDisk(self, (SenseDiskRequest*)req);
                break;

            default:
                req->status = EINVAL;
                break;
        }
    }
}


static void _req_trampoline(IORequest* _Nonnull req)
{
    DiskDriver_HandleRequest(req->driver, req);
}

errno_t DiskDriver_beginIO(DiskDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    req->item.func = (dispatch_item_func_t)_req_trampoline;
    req->driver = self;

    return dispatch_item_async(self->dq, 0, (dispatch_item_t)req);
}

errno_t DiskDriver_doIO(DiskDriverRef _Nonnull self, IORequest* _Nonnull req)
{
    req->item.func = (dispatch_item_func_t)_req_trampoline;
    req->driver = self;

    errno_t err = dispatch_item_sync(self->dq, (dispatch_item_t)req);
    if (err == EOK) {
        err = req->status;
    }

    return err;
}


errno_t DiskDriver_FormatDisk(DiskDriverRef _Nonnull self, IOChannelRef _Nonnull ch, char fillByte)
{
    decl_try_err();
    FormatDiskRequest r;

    IORequest_Init(&r, kDiskRequest_FormatDisk);
    r.fillByte = fillByte;

    return DiskDriver_DoIO(self, (IORequest*)&r);
}

errno_t DiskDriver_FormatTrack(DiskDriverRef _Nonnull self, IOChannelRef _Nonnull ch, char fillByte)
{
    decl_try_err();
    FormatTrackRequest r;

    IORequest_Init(&r, kDiskRequest_FormatTrack);
    r.offset = IOChannel_GetOffset(ch);
    r.fillByte = fillByte;
    r.resCount = 0;

    err = DiskDriver_DoIO(self, (IORequest*)&r);
    if (err == EOK) {
        IOChannel_IncrementOffsetBy(ch, r.resCount);
    }

    return err;
}

errno_t DiskDriver_GetDriveInfo(DiskDriverRef _Nonnull self, drive_info_t* _Nonnull info)
{
    GetDriveInfoRequest r;

    IORequest_Init(&r, kDiskRequest_GetDriveInfo);
    r.ip = info;

    return DiskDriver_DoIO(self, (IORequest*)&r);
}

errno_t DiskDriver_GetDiskInfo(DiskDriverRef _Nonnull self, disk_info_t* _Nonnull info)
{
    DiskGeometryRequest r;

    IORequest_Init(&r, kDiskRequest_GetDiskInfo);
    r.gp = info;

    return DiskDriver_DoIO(self, (IORequest*)&r);
}


//
// MARK: -
// I/O Channel API
//

off_t DiskDriver_getSeekableRange(DiskDriverRef _Nonnull self)
{
    decl_try_err();
    disk_info_t info;
    off_t rng;

    //XXX integrate with new locking model
//    mtx_lock(&self->mtx);
    err = DiskDriver_GetDiskInfo(self, &info);
    if (err == EOK) {
        rng = (off_t)info.sectorsPerDisk * (off_t)info.sectorSize;
    }
    else {
        rng = 0ll;
    }
//    mtx_unlock(&self->mtx);
    return rng;
}

static errno_t _DiskDriver_rdwr(DiskDriverRef _Nonnull self, int type, IOChannelRef _Nonnull ch, void* _Nonnull buf, ssize_t byteCount, ssize_t* _Nonnull pOutByteCount)
{
    decl_try_err();
    StrategyRequest r;

    IORequest_Init(&r, type);
    r.offset = IOChannel_GetOffset(ch);
    r.options = 0;
    r.iovCount = 1;
    r.iov[0].data = buf;
    r.iov[0].size = byteCount;
    r.resCount = 0;

    err = DiskDriver_DoIO(self, (IORequest*)&r);

    if (r.resCount > 0) {
        IOChannel_IncrementOffsetBy(ch, r.resCount);
    }

    *pOutByteCount = r.resCount;
    return err;
}

errno_t DiskDriver_read(DiskDriverRef _Nonnull self, IOChannelRef _Nonnull ch, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    return _DiskDriver_rdwr(self, kDiskRequest_Read, ch, buf, nBytesToRead, pOutBytesRead);
}

errno_t DiskDriver_write(DiskDriverRef _Nonnull self, IOChannelRef _Nonnull ch, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    return _DiskDriver_rdwr(self, kDiskRequest_Write, ch, buf, nBytesToWrite, pOutBytesWritten);
}

errno_t DiskDriver_ioctl(DiskDriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kDiskCommand_GetDriveInfo: {
            drive_info_t* info = va_arg(ap, drive_info_t*);
            
            return DiskDriver_GetDriveInfo(self, info);
        }

        case kDiskCommand_GetDiskInfo: {
            disk_info_t* info = va_arg(ap, disk_info_t*);
        
            return DiskDriver_GetDiskInfo(self, info);
        }

        case kDiskCommand_FormatDisk: {
            const char fillByte = va_arg(ap, int);

            const errno_t err = DiskDriver_FormatDisk(self, pChannel, fillByte);
            return err;
        }

        case kDiskCommand_FormatTrack: {
            const char fillByte = va_arg(ap, int);

            return DiskDriver_FormatTrack(self, pChannel, fillByte);
        }

        case kDiskCommand_SenseDisk:
            return DiskDriver_SenseDisk(self);

        default:
            return super_n(ioctl, Driver, DiskDriver, self, pChannel, cmd, ap);
    }
}


class_func_defs(DiskDriver, Driver,
override_func_def(deinit, DiskDriver, Object)
func_def(createDispatchQueue, DiskDriver)
override_func_def(onStop, DiskDriver, Driver)
func_def(beginIO, DiskDriver)
func_def(doIO, DiskDriver)
func_def(handleRequest, DiskDriver)
func_def(getSector, DiskDriver)
func_def(putSector, DiskDriver)
func_def(doFormatDisk, DiskDriver)
func_def(doFormatTrack, DiskDriver)
func_def(doSenseDisk, DiskDriver)
override_func_def(read, DiskDriver, Driver)
override_func_def(write, DiskDriver, Driver)
override_func_def(getSeekableRange, DiskDriver, Driver)
override_func_def(ioctl, DiskDriver, Driver)
);
