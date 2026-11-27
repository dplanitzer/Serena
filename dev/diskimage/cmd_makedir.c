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

static errno_t create_directory(dd_fs_t _Nonnull fs, const char* _Nonnull path, fs_perms_t perms, uid_t uid, gid_t gid)
{
    errno = 0;

    if (!dd_makedir(fs, path, perms)) {
        if (dd_chown(fs, path, uid, gid) < 0) {
            dd_remove(fs, path);
        }
    }
    return errno;
}

static errno_t create_directory_recursively(dd_fs_t _Nonnull fs, const char* _Nonnull path, fs_perms_t permissions, uid_t uid, gid_t gid)
{
    decl_try_err();
    dd_path_iter_t it = DD_PATH_ITER_INITIALIZER;

    // Iterate through the path string
    while (!dd_path_next(&it, path)) {
        char* p = (char*)it.end;

        if (*p == '/') {
            *p = '\0';

            err = create_directory(fs, path, permissions, uid, gid);
            if (err != EOK) {
                // Stop if the intermediate "directory" is actually a file
                if (err == EEXIST && dd_path_kind(fs, path) == DD_KIND_OTHER) {
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
        err = create_directory(fs, path, permissions, uid, gid);
    }

    return err;
}


////////////////////////////////////////////////////////////////////////////////


errno_t cmd_makedir(const char* _Nonnull dmgPath, bool shouldCreateParents, fs_perms_t dirPerms, uid_t uid, gid_t gid, const clap_string_array_t* _Nonnull paths)
{
    decl_try_err();
    dd_fs_t fs = NULL;

    if (paths->count < 1) {
        return ERANGE;
    }

    try_null(fs, dd_mount(dmgPath), errno);

    for (size_t i = 0; i < paths->count; i++) {
        const char* cur_path = paths->strings[i];

        try(dd_validate_path(cur_path));

        if (shouldCreateParents) {
            try(create_directory_recursively(fs, cur_path, dirPerms, uid, gid));
        }
        else {
            try(create_directory(fs, cur_path, dirPerms, uid, gid));
        }
    }

catch:
    dd_unmount(fs, (err == 0) ? DD_USYNC : 0);
    return err;
}
