//
//  cmd_format.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "RamContainer.h"
#include <string.h>
#include <filesystem/serenafs/tools/format.h>


static errno_t block_write(intptr_t fd, const void* _Nonnull buf, LogicalBlockAddress blockAddr, size_t blockSize)
{
    ssize_t bytesWritten;
    const errno_t err = RamContainer_Write((void*)fd, buf, blockSize, blockAddr * blockSize, &bytesWritten);

    return (err == EOK && bytesWritten == blockSize) ? EOK : EIO;
}


errno_t cmd_format(bool bQuick, FilePermissions rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamContainerRef fsContainer = NULL;

    if (strcmp(fsType, "sefs")) {
        throw(EINVAL);
    }

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &fsContainer));
    
    if (!bQuick) {
        RamContainer_WipeDisk(fsContainer);
    }

    try(sefs_format((intptr_t)fsContainer, block_write, FSContainer_GetBlockCount(fsContainer), FSContainer_GetBlockSize(fsContainer), rootDirUid, rootDirGid, rootDirPerms, label));
    err = RamContainer_WriteToPath(fsContainer, dmgPath);

catch:
    Object_Release(fsContainer);
    return err;
}
