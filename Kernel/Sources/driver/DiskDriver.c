//
//  DiskDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskDriver.h"
#include <dispatchqueue/DispatchQueue.h>

struct IOReadRequest {
    void* _Nonnull          pBuffer;    // in
    LogicalBlockAddress     lba;        // in

    errno_t                 err;        // out
};

struct IOWriteRequest {
    const void* _Nonnull    pBuffer;    // in
    LogicalBlockAddress     lba;        // in

    errno_t                 err;        // out
};

struct IOGetInfoRequest {
    DiskInfo* _Nonnull      pInfo;      // out
    errno_t                 err;        // out
};


static void DiskDriver_getInfoStub(DiskDriverRef _Nonnull self, struct IOGetInfoRequest* _Nonnull rq)
{
    rq->err = invoke_n(getInfoAsync, DiskDriver, self, rq->pInfo);
}

// Returns information about the disk drive and the media loaded into the
// drive.
errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    struct IOGetInfoRequest rq;

    rq.pInfo = pOutInfo;
    rq.err = EOK;

    DispatchQueue_DispatchSyncArgs(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_getInfoStub, self, &rq, sizeof(rq));
    return rq.err;
}

// Returns information about the disk drive and the media loaded into the
// drive.
errno_t DiskDriver_getInfoAsync(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo)
{
    pOutInfo->blockSize = 512;
    pOutInfo->blockCount = 0;
    pOutInfo->isReadOnly = true;
    pOutInfo->isMediaLoaded = false;

    return EOK;
}


static void DiskDriver_getBlockStub(DiskDriverRef _Nonnull self, struct IOReadRequest* _Nonnull rq)
{
    rq->err = invoke_n(getBlockAsync, DiskDriver, self, rq->pBuffer, rq->lba);
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t DiskDriver_GetBlock(DiskDriverRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    struct IOReadRequest rq;

    rq.pBuffer = pBuffer;
    rq.lba = lba;
    rq.err = EOK;

    DispatchQueue_DispatchSyncArgs(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_getBlockStub, self, &rq, sizeof(rq));
    return rq.err;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t DiskDriver_getBlockAsync(DiskDriverRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    return EIO;
}


static void DiskDriver_putBlockStub(DiskDriverRef _Nonnull self, struct IOWriteRequest* _Nonnull rq)
{
    rq->err = invoke_n(putBlockAsync, DiskDriver, self, rq->pBuffer, rq->lba);
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t DiskDriver_PutBlock(DiskDriverRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    struct IOWriteRequest rq;

    rq.pBuffer = pBuffer;
    rq.lba = lba;
    rq.err = EOK;

    DispatchQueue_DispatchSyncArgs(Driver_GetDispatchQueue(self), (VoidFunc_2)DiskDriver_putBlockStub, self, &rq, sizeof(rq));
    return rq.err;
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t DiskDriver_putBlockAsync(DiskDriverRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
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
