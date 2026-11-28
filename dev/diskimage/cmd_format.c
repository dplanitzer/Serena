//
//  cmd_format.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <dd/RamContainer.h>
#include "sefs_init.h"
#include <string.h>
#include <filesystem/FSUtilities.h>


static errno_t block_write(intptr_t fd, const void* _Nonnull buf, blkno_t blockAddr, size_t blockSize)
{
    ssize_t bytesWritten;
    const errno_t err = RamContainer_Write((void*)fd, buf, blockSize, blockAddr * blockSize, &bytesWritten);

    return (err == EOK && bytesWritten == blockSize) ? EOK : EIO;
}


errno_t cmd_format(const char* _Nonnull dmgPath, bool bQuick, fs_perms_t rootDirPerms, uid_t rootDirUid, gid_t rootDirGid, const char* _Nonnull fsType, const char* _Nonnull label)
{
    decl_try_err();
    RamContainerRef fsContainer = NULL;
    nanotime_t now;

    if (strcmp(fsType, "sefs")) {
        throw(EINVAL);
    }

    try(RamContainer_Open(dmgPath, &fsContainer));
    
    if (!bQuick) {
        RamContainer_WipeDisk(fsContainer);
    }

    FSGetCurrentTime(&now);
    try(sefs_init((intptr_t)fsContainer, block_write, FSContainer_GetBlockCount(fsContainer), FSContainer_GetBlockSize(fsContainer), &now, rootDirUid, rootDirGid, rootDirPerms, label));
    err = RamContainer_Sync(fsContainer);

catch:
    Object_Release(fsContainer);
    return err;
}
