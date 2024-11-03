//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include <dispatchqueue/DispatchQueue.h>

struct IOGetInfoRequest {
    DiskInfo* _Nonnull      pInfo;      // out
    errno_t                 err;        // out
};


static void DiskDriver_getInfoStub(DiskDriverRef _Nonnull self, struct IOGetInfoRequest* _Nonnull rq)
{
    rq->err = invoke_n(getInfoAsync, DiskDriver, self, rq->pInfo);
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
errno_t DiskDriver_getInfoAsync(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    pOutInfo->blockSize = 512;
    pOutInfo->blockCount = 0;
    pOutInfo->mediaId = 0;
    pOutInfo->isReadOnly = true;

    return EOK;
}


static void DiskDriver_getBlockStub(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    pBlock->status = invoke_n(getBlockAsync, DiskDriver, self, pBlock);
}

errno_t DiskDriver_GetBlock(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    DispatchQueue_DispatchClosure(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_getBlockStub, self, pBlock, 0, kDispatchOption_Sync, 0);
    return DiskBlock_GetStatus(pBlock);
}

// Reads the contents of the given block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
// The abstract implementation returns EIO.
errno_t DiskDriver_getBlockAsync(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    return EIO;
}


static void DiskDriver_putBlockStub(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    pBlock->status = invoke_n(putBlockAsync, DiskDriver, self, pBlock);
}

errno_t DiskDriver_PutBlock(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    DispatchQueue_DispatchClosure(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_putBlockStub, self, pBlock, 0, kDispatchOption_Sync, 0);
    return DiskBlock_GetStatus(pBlock);
}

// Writes the contents of 'pBlock' to the corresponding disk block. Blocks
// the caller until the write has completed. The contents of the block on
// disk is left in an indeterminate state of the write fails in the middle
// of the write. The block may contain a mix of old and new data.
// The abstract implementation returns EIO.
errno_t DiskDriver_putBlockAsync(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock)
{
    return EIO;
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
func_def(getInfoAsync, DiskDriver)
func_def(getBlockAsync, DiskDriver)
func_def(putBlockAsync, DiskDriver)
override_func_def(ioctl, DiskDriver, Driver)
);
