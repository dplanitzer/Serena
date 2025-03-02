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

errno_t sefs_format(intptr_t fd, LogicalBlockCount blockCount, size_t blockSize, uid_t uid, gid_t gid, FilePermissions permissions);


errno_t cmd_format(bool bQuick, FilePermissions rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull dmgPath)
{
    decl_try_err();
    FSContainerInfo info;
    RamFSContainerRef fsContainer = NULL;

    if (strcmp(fsType, "sefs")) {
        throw(EINVAL);
    }

    try(RamFSContainer_CreateWithContentsOfPath(dmgPath, &fsContainer));
    
    if (!bQuick) {
        RamFSContainer_WipeDisk(fsContainer);
    }

    try(FSContainer_GetInfo(fsContainer, &info));
    try(sefs_format((intptr_t)fsContainer, info.blockCount, info.blockSize, rootDirUid, rootDirGid, rootDirPerms));
    err = RamFSContainer_WriteToPath(fsContainer, dmgPath);

catch:
    Object_Release(fsContainer);
    return err;
}
