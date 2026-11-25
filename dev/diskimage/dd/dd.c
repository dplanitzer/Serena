//
//  dd.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/16/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "dd.h"
#include "FSManager.h"


int dd_path_kind(FSManagerRef fsm, const char *path)
{
    fs_attr_t attr;
    errno_t err = FileManager_GetAttributes(&fsm->fm, path, &attr);

    if (err != 0) {
        return DD_KIND_ERROR;
    }

    return (attr.file_type == FS_FTYPE_DIR) ? DD_KIND_DIRECTORY : DD_KIND_OTHER;
}
