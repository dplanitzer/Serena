//
//  path_utils_dmg.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/16/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "path_utils.h"
#include "FSManager.h"


int dmg_path_kind(FSManagerRef fsm, const char *path)
{
    fs_attr_t attr;
    errno_t err = FileManager_GetAttributes(&fsm->fm, path, &attr);

    if (err != 0) {
        return KIND_ERROR;
    }

    return (attr.file_type == FS_FTYPE_DIR) ? KIND_DIRECTORY : KIND_OTHER;
}
