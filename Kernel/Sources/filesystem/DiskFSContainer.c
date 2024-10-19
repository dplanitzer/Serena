//
//  DiskFSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskFSContainer.h"
#include <driver/disk/DiskDriver.h>


final_class_ivars(DiskFSContainer, FSContainer,
    DiskDriverRef _Nonnull  diskDriver;
);


errno_t DiskFSContainer_Create(DiskDriverRef _Nonnull pDriver, FSContainerRef _Nullable * _Nonnull pOutContainer)
{
    decl_try_err();
    struct DiskFSContainer* self = NULL;

    if ((err = Object_Create(DiskFSContainer, (struct DiskFSContainer*)&self)) == EOK) {
        self->diskDriver = Object_RetainAs(pDriver, DiskDriver);
    }

    *pOutContainer = (FSContainerRef)self;
    return err;
}

void DiskFSContainer_deinit(struct DiskFSContainer* _Nonnull self)
{
    Object_Release(self->diskDriver);
    self->diskDriver = NULL;
}

errno_t DiskFSContainer_getInfo(struct DiskFSContainer* _Nonnull self, FSContainerInfo* pOutInfo)
{
    DiskInfo di;
    const errno_t err = DiskDriver_GetInfo(self->diskDriver, &di);

    pOutInfo->blockSize = di.blockSize;
    pOutInfo->blockCount = di.blockCount;
    pOutInfo->mediaId = di.mediaId;
    pOutInfo->isReadOnly = di.isReadOnly;

    return err;
}

errno_t DiskFSContainer_getBlock(struct DiskFSContainer* _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    return DiskDriver_GetBlock(self->diskDriver, pBuffer, lba);
}

errno_t DiskFSContainer_putBlock(struct DiskFSContainer* _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    return DiskDriver_PutBlock(self->diskDriver, pBuffer, lba);
}


class_func_defs(DiskFSContainer, Object,
override_func_def(deinit, DiskFSContainer, Object)
override_func_def(getInfo, DiskFSContainer, FSContainer)
override_func_def(getBlock, DiskFSContainer, FSContainer)
override_func_def(putBlock, DiskFSContainer, FSContainer)
);
