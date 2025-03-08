//
//  cmd_delete.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "FSManager.h"


errno_t cmd_delete(const char* _Nonnull path, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamFSContainerRef disk = NULL;
    FSManagerRef m = NULL;

    try(RamFSContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &m));
    
    try(FileManager_Unlink(&m->fm, path));

catch:
    FSManager_Destroy(m);
    if (err == EOK) {
        err = RamFSContainer_WriteToPath(disk, dmgPath);
    }
    Object_Release(disk);
    return err;
}
