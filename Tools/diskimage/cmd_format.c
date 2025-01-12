//
//  cmd_format.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "RamFSContainer.h"
#include <string.h>


errno_t cmd_format(bool bQuick, FilePermissions rootDirPerms, UserId rootDirUid, GroupId rootDirGid, const char* _Nonnull fsType, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamFSContainerRef fsContainer = NULL;

    if (strcmp(fsType, "sefs")) {
        throw(EINVAL);
    }

    try(RamFSContainer_CreateWithContentsOfPath(dmgPath, &fsContainer));
    
    if (!bQuick) {
        RamFSContainer_WipeDisk(fsContainer);
    }

    try(SerenaFS_FormatDrive((FSContainerRef)fsContainer, rootDirUid, rootDirGid, rootDirPerms));
    err = RamFSContainer_WriteToPath(fsContainer, dmgPath);

catch:
    Object_Release(fsContainer);
    return err;
}
