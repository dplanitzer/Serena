//
//  DiskFSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskFSContainer.h"
#include <driver/DriverCatalog.h>
#include <driver/disk/DiskDriver.h>


final_class_ivars(DiskFSContainer, FSContainer,
    DriverId    driverId;
);


errno_t DiskFSContainer_Create(DriverId driverId, FSContainerRef _Nullable * _Nonnull pOutContainer)
{
    decl_try_err();
    struct DiskFSContainer* self = NULL;

    if ((err = Object_Create(DiskFSContainer, (struct DiskFSContainer*)&self)) == EOK) {
        self->driverId = driverId;
    }

    *pOutContainer = (FSContainerRef)self;
    return err;
}

errno_t DiskFSContainer_getInfo(struct DiskFSContainer* _Nonnull self, FSContainerInfo* pOutInfo)
{
    decl_try_err();
    DiskDriverRef pDriver;
    DiskInfo di;

    if ((pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, self->driverId)) != NULL) {
        if ((err = DiskDriver_GetInfo(pDriver, &di)) == EOK) {
            pOutInfo->blockSize = di.blockSize;
            pOutInfo->blockCount = di.blockCount;
            pOutInfo->mediaId = di.mediaId;
            pOutInfo->isReadOnly = di.isReadOnly;
        }
        Object_Release(pDriver);
    }
    else {
        err = ENODEV;
    }

    return err;
}

// Acquires the disk block with the block address 'lba'. The acquisition is
// done according to the acquisition mode 'mode'. An error is returned if
// the disk block needed to be loaded and loading failed for some reason.
// Once done with the block, it must be relinquished by calling the
// relinquishBlock() method.
errno_t DiskFSContainer_acquireBlock(struct DiskFSContainer* _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskDriverRef pDriver;
    DiskBlockRef pBlock = NULL;

    if ((pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, self->driverId)) != NULL) {
        err = DiskBlock_Create(self->driverId, 1, lba, &pBlock);
        if (err == EOK) {
            switch (mode) {
                case kAcquireBlock_ReadOnly:
                case kAcquireBlock_Update:
                    err = DiskDriver_GetBlock(pDriver, pBlock);
                    break;

                case kAcquireBlock_Replace:
                    // Caller will overwrite all data cached in the block
                    break;

                case kAcquireBlock_Cleared:
                    memset(DiskBlock_GetMutableData(pBlock), 0, DiskBlock_GetByteSize(pBlock));
                    break; 

            }
        }
        Object_Release(pDriver);
    }
    else {
        err = ENODEV;
    }
    *pOutBlock = pBlock;

    return err;
}

// Relinquishes the disk block 'pBlock' without writing it back to disk.
void DiskFSContainer_relinquishBlock(struct DiskFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock)
{
    if (pBlock) {
        DiskBlock_Destroy(pBlock);
    }
}

// Relinquishes the disk block 'pBlock' and writes the disk block back to
// disk according to the write back mode 'mode'.
errno_t DiskFSContainer_relinquishBlockWriting(struct DiskFSContainer* _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    decl_try_err();
    DiskDriverRef pDriver;

    if (pBlock) {
        if ((pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, self->driverId)) != NULL) {
            switch (mode) {
                case kWriteBlock_Sync:
                    err = DiskDriver_PutBlock(pDriver, pBlock);
                    break;

                case kWriteBlock_Async:
                case kWriteBlock_Deferred:
                    abort();
                    break;
            }
            Object_Release(pDriver);
        }
        else {
            err = ENODEV;
        }

        DiskBlock_Destroy(pBlock);
    }

    return err;
}


class_func_defs(DiskFSContainer, Object,
override_func_def(getInfo, DiskFSContainer, FSContainer)
override_func_def(acquireBlock, DiskFSContainer, FSContainer)
override_func_def(relinquishBlock, DiskFSContainer, FSContainer)
override_func_def(relinquishBlockWriting, DiskFSContainer, FSContainer)
);
