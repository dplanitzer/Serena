//
//  cmd_create.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/19/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "RamFSContainer.h"
#include <stdio.h>


errno_t cmd_create(const DiskImageFormat* _Nonnull dmgFmt, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamFSContainerRef fsContainer;
    bool doCreateDiskImage = true;

    err = RamFSContainer_CreateWithContentsOfPath(dmgPath, &fsContainer);
    if (err == EOK) {
        if (dmgFmt->format == fsContainer->format && dmgFmt->blockSize == fsContainer->blockSize && dmgFmt->blocksPerDisk == fsContainer->blockCount) {
            doCreateDiskImage = false;
        }
    }

    if (doCreateDiskImage) {
        remove(dmgPath);
        try(RamFSContainer_Create(dmgFmt, &fsContainer));
        try(RamFSContainer_WriteToPath(fsContainer, dmgPath));
    }

catch:
    Object_Release(fsContainer);
    return err;
}
