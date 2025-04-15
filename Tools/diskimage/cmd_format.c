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

errno_t sefs_format(intptr_t fd, LogicalBlockCount blockCount, size_t blockSize, uid_t uid, gid_t gid, FilePermissions permissions, const char* _Nonnull label);


errno_t cmd_format(bool bQuick, FilePermissions rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label, const char* _Nonnull dmgPath)
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

    try(sefs_format((intptr_t)fsContainer, FSContainer_GetBlockCount(fsContainer), FSContainer_GetBlockSize(fsContainer), rootDirUid, rootDirGid, rootDirPerms, label));
    err = RamFSContainer_WriteToPath(fsContainer, dmgPath);

catch:
    Object_Release(fsContainer);
    return err;
}
