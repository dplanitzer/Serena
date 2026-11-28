//
//  cmd_create.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/19/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <dd/RamContainer.h>


errno_t cmd_create(const char* _Nonnull dmgPath, const DiskImageFormat* _Nonnull dmgFmt, bool bOverrideExisting)
{
    decl_try_err();
    RamContainerRef fsContainer;
    const int flags = (bOverrideExisting) ? kRamContainer_OverwriteExisting : 0;

    err = RamContainer_Create(dmgPath, dmgFmt, flags, &fsContainer);
    if (err == EOK) {
        err = RamContainer_Sync(fsContainer);
    }

    Object_Release(fsContainer);
    return err;
}
