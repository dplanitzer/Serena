//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"


errno_t DiskDriver_Create(Class* _Nonnull pClass, unsigned options, const iocat_t* _Nonnull cats, const drive_info_t* _Nonnull driveInfo, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskDriverRef self = NULL;

    try(IODriver_Create(pClass, kIODriver_Exclusive, cats, (IODriverRef*)&self));
    try(DiskDriver_CreateDispatchQueue(self, &self->dq));
    self->driveInfo = *driveInfo;

    // A disk driver always starts out in "no disk in drive" and "disk change is
    // pending" state until reset() triggered by onStart() figures out what the
    // truth really is. Whether there's no disk in the drive or there is one and
    // what the geometry of that disk is. Note that this is the case even for
    // rigid/fixed platter disk drives.
    DiskDriver_NoteSensedDisk(self, NULL);
    self->flags.isDiskChangeActive = 1;

    *pOutSelf = (IODriverRef)self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void DiskDriver_deinit(DiskDriverRef _Nonnull self)
{
    if (self->dq) {
        kdispatch_destroy(self->dq);
        self->dq = NULL;
    }
}

errno_t DiskDriver_createDispatchQueue(DiskDriverRef _Nonnull self, kdispatch_t _Nullable * _Nonnull pOutQueue)
{
    kdispatch_attr_t attr = KDISPATCH_ATTR_INIT_SERIAL_URGENT(KDISPATCH_PRI_NORMAL, "disk");

    return kdispatch_create(&attr, pOutQueue);
}

int DiskDriver_getBootPriority(DiskDriverRef _Nonnull self)
{
    return -1;
}

static void _req_trampoline(IODiskCommand* _Nonnull req)
{
    DiskDriver_DoCommand(req->driver, req);
}

static errno_t DiskDriver_BeginIO(DiskDriverRef _Nonnull self, IODiskCommand* _Nonnull req)
{
    req->u.item.func = (kdispatch_item_func_t)_req_trampoline;
    req->driver = self;

    return kdispatch_item_async(self->dq, 0, (kdispatch_item_t)req);
}

static errno_t DiskDriver_DoIO(DiskDriverRef _Nonnull self, IODiskCommand* _Nonnull req)
{
    req->u.item.func = (kdispatch_item_func_t)_req_trampoline;
    req->driver = self;

    errno_t err = kdispatch_item_sync(self->dq, (kdispatch_item_t)req);
    if (err == EOK) {
        err = req->status;
    }

    return err;
}

bool DiskDriver_stop(DiskDriverRef _Nonnull _Locked self)
{
    if (self->dq) {
        kdispatch_terminate(self->dq, KDISPATCH_TERMINATE_AWAIT_ALL);
    }

    return true;
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
    
        self->diskFlags = info->flags;
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
    
        self->diskFlags = DISK_FLAG_READ_ONLY;
        self->flags.hasDisk = 0;
        self->flags.isDiskChangeActive = 0;
    }
}

void DiskDriver_doSenseDisk(DiskDriverRef _Nonnull self, IOSenseDiskCommand* _Nonnull req)
{

}

errno_t DiskDriver_SenseDisk(DiskDriverRef _Nonnull self)
{
    IOSenseDiskCommand cmd;

    IODiskCommand_Init(&cmd, kIODiskCommand_SenseDisk);
    return DiskDriver_DoIO(self, (IODiskCommand*)&cmd);
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

static void _do_rw(DiskDriverRef _Nonnull self, IORWCommand* _Nonnull iop, bool isAsync)
{
    decl_try_err();
    chs_t chs;
    ssize_t rlen = 0;
    sno_t lsa = iop->offset / (off_t)self->sectorSize;

    for (size_t i = 0; (i < iop->iovcnt) && (err == EOK); i++) {
        const iovec_t* iov = &iop->iov[i];
        ssize_t size = iov->iov_len;
        uint8_t* data = iov->iov_base;

        if (size < 0) {
            err = EINVAL;
            break;
        }
        else if (size > 0 && iop->offset < 0ll) {
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
            else if (iop->s.type == kIODiskCommand_ReadAsync) {
                err = DiskDriver_GetSector(self, &chs, data, self->sectorSize);
            }
            else if (iop->s.type == kIODiskCommand_WriteAsync) {
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
            rlen += self->sectorSize;
            lsa++;
        }
    }


    if (isAsync) {
        iop->u.completion->f(iop->u.completion->ctx, iop->u.completion->arg, (rlen > 0) ? EOK : err, rlen);
    } else {
        iop->u.res.err = (rlen > 0) ? EOK : err;
        iop->u.res.rlen = rlen;
    }
}


errno_t DiskDriver_doFormatDisk(DiskDriverRef _Nonnull self, char fillByte)
{
    return ENOTSUP;
}

errno_t DiskDriver_doFormatTrack(DiskDriverRef _Nonnull self, const chs_t* chs, char fillByte, size_t secSize)
{
    return ENOTSUP;
}

static void _format_track_async(DiskDriverRef _Nonnull self, IOFormatTrackCommand* _Nonnull req)
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
        req->rlen = self->sectorsPerTrack * self->sectorSize;
    }
    req->s.status = err;
}


// Returns information about the disk drive.
static void _get_drive_info_async(DiskDriverRef _Nonnull self, IOGetDriveInfoCommand* _Nonnull req)
{
    drive_info_t* p = req->ip;
    
    p->flags = self->diskFlags;
    req->s.status = EOK;
}

static void _get_disk_info_async(DiskDriverRef _Nonnull self, IOGetDiskInfoCommand* _Nonnull req)
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
        p->flags = self->diskFlags;
        req->s.status = EOK;
    }
    else {
        req->s.status = ENOMEDIUM;
    }
}


void DiskDriver_doCommand(DiskDriverRef _Nonnull self, IODiskCommand* _Nonnull req)
{
    switch (req->type) {
        case kIODiskCommand_Read:
        case kIODiskCommand_Write:
            _do_rw(self, (IORWCommand*)req, false);
            break;

        case kIODiskCommand_ReadAsync:
        case kIODiskCommand_WriteAsync:
            _do_rw(self, (IORWCommand*)req, true);
            break;

        case kIODiskCommand_FormatDisk:
            req->status = DiskDriver_DoFormatDisk(self, ((IOFormatDiskCommand*)req)->fillByte);
            break;

        case kIODiskCommand_FormatTrack:
            _format_track_async(self, (IOFormatTrackCommand*)req);
            break;

        case kIODiskCommand_GetDriveInfo:
            _get_drive_info_async(self, (IOGetDriveInfoCommand*)req);
            break;

        case kIODiskCommand_GetDiskInfo:
            _get_disk_info_async(self, (IOGetDiskInfoCommand*)req);
            break;

        case kIODiskCommand_SenseDisk:
            DiskDriver_DoSenseDisk(self, (IOSenseDiskCommand*)req);
            break;

        default:
            req->status = EINVAL;
            break;
    }
}


errno_t DiskDriver_FormatDisk(DiskDriverRef _Nonnull self, char fillByte)
{
    IOFormatDiskCommand cmd;

    IODiskCommand_Init(&cmd, kIODiskCommand_FormatDisk);
    cmd.fillByte = fillByte;

    return DiskDriver_DoIO(self, (IODiskCommand*)&cmd);
}

errno_t DiskDriver_FormatTrack(DiskDriverRef _Nonnull self, off_t* _Nonnull pOffset, char fillByte)
{
    decl_try_err();
    IOFormatTrackCommand cmd;

    IODiskCommand_Init(&cmd, kIODiskCommand_FormatTrack);
    cmd.offset = *pOffset;
    cmd.fillByte = fillByte;
    cmd.rlen = 0;

    err = DiskDriver_DoIO(self, (IODiskCommand*)&cmd);
    if (err == EOK) {
        *pOffset = cmd.rlen;
    }

    return err;
}

errno_t DiskDriver_GetDriveInfo(DiskDriverRef _Nonnull self, drive_info_t* _Nonnull info)
{
    IOGetDriveInfoCommand cmd;

    IODiskCommand_Init(&cmd, kIODiskCommand_GetDriveInfo);
    cmd.ip = info;

    return DiskDriver_DoIO(self, (IODiskCommand*)&cmd);
}

errno_t DiskDriver_GetDiskInfo(DiskDriverRef _Nonnull self, disk_info_t* _Nonnull info)
{
    IOGetDiskInfoCommand cmd;

    IODiskCommand_Init(&cmd, kIODiskCommand_GetDiskInfo);
    cmd.gp = info;

    return DiskDriver_DoIO(self, (IODiskCommand*)&cmd);
}


//
// MARK: -
// I/O Handler API
//

off_t DiskDriver_getSeekableSize(DiskDriverRef _Nonnull self)
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

static errno_t _DiskDriver_rdwr(DiskDriverRef _Nonnull self, int type, fd_flags_t flags, off_t* _Nonnull pOffset, void* _Nonnull buf, ssize_t byteCount, ssize_t* _Nonnull pOutByteCount)
{
    decl_try_err();
    IORWCommand p;
    iovec_t iov;

    iov.iov_base = buf;
    iov.iov_len = byteCount;

    IODiskCommand_Init(&p, type);
    p.offset = *pOffset;
    p.iovcnt = 1;
    p.iov = &iov;

    err = DiskDriver_DoIO(self, (IODiskCommand*)&p);

    if (p.u.res.rlen > 0) {
        *pOffset += p.u.res.rlen;
    }

    *pOutByteCount = p.u.res.rlen;
    return err;
}

errno_t DiskDriver_read(DiskDriverRef _Nonnull self, fd_flags_t flags, off_t* _Nonnull pOffset, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    return _DiskDriver_rdwr(self, kIODiskCommand_Read, flags, pOffset, buf, nBytesToRead, pOutBytesRead);
}

errno_t DiskDriver_write(DiskDriverRef _Nonnull self, fd_flags_t flags, off_t* _Nonnull pOffset, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    return _DiskDriver_rdwr(self, kIODiskCommand_Write, flags, pOffset, buf, nBytesToWrite, pOutBytesWritten);
}


errno_t DiskDriver_ReadAsync(DiskDriverRef _Nonnull self, const iovec_t* _Nonnull iov, int iovcnt, off_t offset, const IOCompletion* _Nonnull completion)
{
    decl_try_err();
    IORWCommand* p;

    err = IODiskCommand_Get(sizeof(IORWCommand), (IODiskCommand**)&p);
    if (err != EOK) {
        return err;
    }

    IODiskCommand_Init(p, kIODiskCommand_ReadAsync);
    p->s.u.item.retireFunc = (kdispatch_retire_func_t)IODiskCommand_Put;
    p->iovcnt = iovcnt;
    p->iov = iov;
    p->offset = offset;
    p->u.completion = completion;

    return DiskDriver_BeginIO(self, (IODiskCommand*)p);
}

errno_t DiskDriver_WriteAsync(DiskDriverRef _Nonnull self, const iovec_t* _Nonnull iov, int iovcnt, off_t offset, const IOCompletion* _Nonnull completion)
{
    decl_try_err();
    IORWCommand* p;

    err = IODiskCommand_Get(sizeof(IORWCommand), (IODiskCommand**)&p);
    if (err != EOK) {
        return err;
    }

    IODiskCommand_Init(p, kIODiskCommand_WriteAsync);
    p->s.u.item.retireFunc = (kdispatch_retire_func_t)IODiskCommand_Put;
    p->iovcnt = iovcnt;
    p->iov = iov;
    p->offset = offset;
    p->u.completion = completion;

    return DiskDriver_BeginIO(self, (IODiskCommand*)p);
}


class_func_defs(DiskDriver, IODriver,
override_func_def(deinit, DiskDriver, Object)
override_func_def(stop, DiskDriver, IODriver)
func_def(createDispatchQueue, DiskDriver)
func_def(getBootPriority, DiskDriver)
func_def(read, DiskDriver)
func_def(write, DiskDriver)
func_def(getSeekableSize, DiskDriver)
func_def(doCommand, DiskDriver)
func_def(getSector, DiskDriver)
func_def(putSector, DiskDriver)
func_def(doFormatDisk, DiskDriver)
func_def(doFormatTrack, DiskDriver)
func_def(doSenseDisk, DiskDriver)
);
