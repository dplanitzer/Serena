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

errno_t DiskFSContainer_getBlock(struct DiskFSContainer* _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    decl_try_err();
    DiskDriverRef pDriver;

    if ((pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, self->driverId)) != NULL) {
        err = DiskDriver_GetBlock(pDriver, pBuffer, lba);
        Object_Release(pDriver);
    }
    else {
        err = ENODEV;
    }

    return err;
}

errno_t DiskFSContainer_putBlock(struct DiskFSContainer* _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
        decl_try_err();
    DiskDriverRef pDriver;

    if ((pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, self->driverId)) != NULL) {
        err = DiskDriver_PutBlock(pDriver, pBuffer, lba);
        Object_Release(pDriver);
    }
    else {
        err = ENODEV;
    }

    return err;
}


class_func_defs(DiskFSContainer, Object,
override_func_def(getInfo, DiskFSContainer, FSContainer)
override_func_def(getBlock, DiskFSContainer, FSContainer)
override_func_def(putBlock, DiskFSContainer, FSContainer)
);
