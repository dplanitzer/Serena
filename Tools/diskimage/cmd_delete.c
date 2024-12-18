//
//  cmd_delete.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "DiskController.h"


errno_t cmd_delete(const char* _Nonnull path, const char* _Nonnull dmgPath)
{
    decl_try_err();
    DiskControllerRef self;

    try(DiskController_CreateWithContentsOfPath(dmgPath, &self));
    try(FileManager_Unlink(&self->fm, path));
    err = DiskController_WriteToPath(self, dmgPath);

catch:
    DiskController_Destroy(self);
    return err;
}
