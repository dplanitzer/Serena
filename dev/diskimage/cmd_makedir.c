//
//  cmd_makedir.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <string.h>
#include <dd/dd.h>
#include <kpi/file.h>

static errno_t create_directory(FileManagerRef _Nonnull fm, const char* _Nonnull path, fs_perms_t perms, uid_t uid, gid_t gid)
{
    decl_try_err();

    err = FileManager_CreateDirectory(fm, path, perms);
    if (err == EOK) {
        err = FileManager_SetFileOwner(fm, path, uid, gid);

        if (err != EOK) {
            FileManager_Unlink(fm, path, __FS_ULNK_ANY);
        }
    }

    return err;
}

static errno_t create_directory_recursively(FileManagerRef fm, const char* _Nonnull path, fs_perms_t permissions, uid_t uid, gid_t gid)
{
    decl_try_err();
    dd_path_iter_t it = DD_PATH_ITER_INITIALIZER;
    fs_attr_t attr;

    // Iterate through the path string
    while (!dd_path_next(&it, path)) {
        char* p = (char*)it.end;

        if (*p == '/') {
            *p = '\0';

            err = create_directory(fm, path, permissions, uid, gid);
            if (err != EOK) {
                // Stop if the intermediate "directory" is actually a file
                if (err == EEXIST && FileManager_GetAttributes(fm, path, &attr) == EOK && attr.file_type != FS_FTYPE_DIR) {
                    err = ENOTDIR;
                    break;
                }
            }

            *p = '/';
            err = EOK;
        }
    }

    // Create the final target directory
    if (err == EOK) {
        err = create_directory(fm, path, permissions, uid, gid);
    }

    return err;
}


////////////////////////////////////////////////////////////////////////////////


errno_t cmd_makedir(const char* _Nonnull dmgPath, bool shouldCreateParents, fs_perms_t dirPerms, uid_t uid, gid_t gid, const clap_string_array_t* _Nonnull paths)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef m = NULL;

    if (paths->count < 1) {
        return ERANGE;
    }

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &m));

    for (size_t i = 0; i < paths->count; i++) {
        const char* cur_path = paths->strings[i];

        try(dd_validate_path(cur_path));

        if (shouldCreateParents) {
            try(create_directory_recursively(&m->fm, cur_path, dirPerms, uid, gid));
        }
        else {
            try(create_directory(&m->fm, cur_path, dirPerms, uid, gid));
        }
    }

catch:
    FSManager_Destroy(m);
    if (err == EOK) {
        err = RamContainer_WriteToPath(disk, dmgPath);
    }
    Object_Release(disk);
    return err;
}
