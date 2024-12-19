//
//  cmd_format.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "DiskController.h"
#include <string.h>


errno_t cmd_format(bool bQuick, FilePermissions rootDirPerms, User rootDirOwner, const char* _Nonnull fsType, const char* _Nonnull dmgPath)
{
    decl_try_err();
    DiskControllerRef self;

    if (strcmp(fsType, "sefs")) {
        throw(EINVAL);
    }

    try(DiskController_CreateWithContentsOfPath(dmgPath, &self));
    
    if (!bQuick) {
        RamFSContainer_WipeDisk(self->fsContainer);
    }

    try(SerenaFS_FormatDrive((FSContainerRef)self->fsContainer, rootDirOwner, rootDirPerms));
    err = DiskController_WriteToPath(self, dmgPath);

catch:
    DiskController_Destroy(self);
    return err;
}
