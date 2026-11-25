//
//  dd.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/16/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "dd.h"
#include "FSManager.h"
#include <errno.h>


int dd_path_kind(FSManagerRef fsm, const char *path)
{
    fs_attr_t attr;
    errno_t err = FileManager_GetAttributes(&fsm->fm, path, &attr);

    if (err != EOK) {
        errno = err;
        return DD_KIND_ERROR;
    }

    return (attr.file_type == FS_FTYPE_DIR) ? DD_KIND_DIRECTORY : DD_KIND_OTHER;
}

int dd_makedir(FSManagerRef fsm, const char *path, fs_perms_t perms)
{
    const errno_t err = FileManager_CreateDirectory(&fsm->fm, path, perms);
    
    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_remove(FSManagerRef fsm, const char *path)
{
    const errno_t err = FileManager_Unlink(&fsm->fm, path, __FS_ULNK_ANY);

    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_chown(FSManagerRef fsm, const char *path, uid_t uid, gid_t gid)
{
    const errno_t err = FileManager_SetFileOwner(&fsm->fm, path, uid, gid);

    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_attr(FSManagerRef fsm, const char *path, fs_attr_t *attr)
{
    const errno_t err = FileManager_GetAttributes(&fsm->fm, path, attr);

    if (err == EOK) {
        return 0;
    } 
    else {
        errno = err;
        return -1;
    }
}
