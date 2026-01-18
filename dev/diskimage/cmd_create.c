//
//  cmd_create.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/19/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "RamContainer.h"
#include <stdio.h>


errno_t cmd_create(const DiskImageFormat* _Nonnull dmgFmt, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamContainerRef fsContainer;
    bool doCreateDiskImage = true;

    err = RamContainer_CreateWithContentsOfPath(dmgPath, &fsContainer);
    if (err == EOK) {
        if (dmgFmt->format == fsContainer->format
            && dmgFmt->blockSize == FSContainer_GetBlockSize(fsContainer)
            && dmgFmt->blocksPerDisk == FSContainer_GetBlockCount(fsContainer)) {
            doCreateDiskImage = false;
        }
    }

    if (doCreateDiskImage) {
        remove(dmgPath);
        try(RamContainer_Create(dmgFmt, &fsContainer));
        try(RamContainer_WriteToPath(fsContainer, dmgPath));
    }

catch:
    Object_Release(fsContainer);
    return err;
}
