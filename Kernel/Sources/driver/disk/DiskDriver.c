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

struct IOGetInfoRequest {
    DiskInfo* _Nonnull      pInfo;      // out
    errno_t                 err;        // out
};


static void DiskDriver_getInfoStub(DiskDriverRef _Nonnull self, struct IOGetInfoRequest* _Nonnull rq)
{
    rq->err = invoke_n(getInfo_async, DiskDriver, self, rq->pInfo);
}

errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    struct IOGetInfoRequest rq;

    rq.pInfo = pOutInfo;
    rq.err = EOK;

    DispatchQueue_DispatchClosure(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_getInfoStub, self, &rq, 0, kDispatchOption_Sync, 0);
    return rq.err;
}

// Returns information about the disk drive and the media loaded into the
// drive.
errno_t DiskDriver_getInfo_async(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    pOutInfo->blockSize = 512;
    pOutInfo->blockCount = 0;
    pOutInfo->mediaId = kMediaId_None;
    pOutInfo->isReadOnly = true;

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

    if (DiskBlock_GetMediaId(pBlock) == DiskDriver_GetCurrentMediaId(self)) {
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
func_def(getInfo_async, DiskDriver)
func_def(getCurrentMediaId, DiskDriver)
func_def(beginIO_async, DiskDriver)
func_def(getBlock, DiskDriver)
func_def(putBlock, DiskDriver)
func_def(endIO, DiskDriver)
override_func_def(ioctl, DiskDriver, Driver)
);
