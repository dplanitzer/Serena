//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include <disk/DiskCache.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverChannel.h>

struct GetInfoReq {
    DiskInfo* _Nonnull  info;       // out
    errno_t             err;        // out
};

struct BeginIOReq {
    DiskBlockRef _Nonnull   block;
    DiskAddress             addr;
};


// Publishes the driver instance to the driver catalog with the given name.
errno_t DiskDriver_publish(DiskDriverRef _Nonnull self, const char* name, intptr_t arg)
{
    decl_try_err();

    if ((err = super_n(publish, Driver, DiskDriver, self, name, arg)) == EOK) {
        if ((err = DiskCache_RegisterDisk(gDiskCache, self, &self->diskId)) != EOK) {
            Driver_Unpublish(self);
        }
    }
    return err;
}

// Removes the driver instance from the driver catalog.
void DiskDriver_unpublish(DiskDriverRef _Nonnull self)
{
    if (self->diskId != kDiskId_None) {
        DiskCache_UnregisterDisk(gDiskCache, self->diskId);
        self->diskId = kDiskId_None;
    }

    super_0(unpublish, Driver, DiskDriver, self);
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


static void DiskDriver_beginIOStub(DiskDriverRef _Nonnull self, struct BeginIOReq* _Nonnull rq)
{
    invoke_n(beginIO_async, DiskDriver, self, rq->block, &rq->addr);
}

errno_t DiskDriver_BeginIO(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock, const DiskAddress* _Nonnull targetAddr)
{
    struct BeginIOReq rq;

    rq.block = pBlock;
    rq.addr = *targetAddr;

    return DispatchQueue_DispatchClosure(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_beginIOStub, self, &rq, sizeof(struct BeginIOReq), 0, 0);
}

void DiskDriver_beginIO_async(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock, const DiskAddress* _Nonnull targetAddr)
{
    decl_try_err();

    if (targetAddr->mediaId == DiskDriver_GetCurrentMediaId(self)) {
        switch (DiskBlock_GetOp(pBlock)) {
            case kDiskBlockOp_Read:
                err = DiskDriver_GetBlock(self, pBlock, targetAddr);
                break;

            case kDiskBlockOp_Write:
                err = DiskDriver_PutBlock(self, pBlock, targetAddr);
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
errno_t DiskDriver_getBlock(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock, const DiskAddress* _Nonnull targetAddr)
{
    return EIO;
}


// Writes the contents of 'pBlock' to the corresponding disk block. Blocks
// the caller until the write has completed. The contents of the block on
// disk is left in an indeterminate state of the write fails in the middle
// of the write. The block may contain a mix of old and new data.
// The abstract implementation returns EIO.
errno_t DiskDriver_putBlock(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock, const DiskAddress* _Nonnull targetAddr)
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
override_func_def(publish, DiskDriver, Driver)
override_func_def(unpublish, DiskDriver, Driver)
func_def(getInfo_async, DiskDriver)
func_def(getCurrentMediaId, DiskDriver)
func_def(beginIO_async, DiskDriver)
func_def(getBlock, DiskDriver)
func_def(putBlock, DiskDriver)
func_def(endIO, DiskDriver)
override_func_def(ioctl, DiskDriver, Driver)
);
