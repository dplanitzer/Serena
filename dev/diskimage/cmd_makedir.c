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

static int create_directory(dd_fs_t _Nonnull fs, const char* _Nonnull path, fs_perms_t perms, uid_t uid, gid_t gid)
{
    errno = 0;

    if (!dd_makedir(fs, path, perms)) {
        if (!dd_chown(fs, path, uid, gid)) {
            return 0;
        }

        dd_remove(fs, path);
    }
    return -1;
}

static int create_directory_recursively(dd_fs_t _Nonnull fs, const char* _Nonnull path, fs_perms_t permissions, uid_t uid, gid_t gid)
{
    dd_path_iter_t it = DD_PATH_ITER_INITIALIZER;

    // Iterate through the path string
    while (!dd_path_next(&it, path)) {
        char* p = (char*)it.end;

        if (*p == '/') {
            *p = '\0';

            if (create_directory(fs, path, permissions, uid, gid) < 0) {
                // Stop if the intermediate "directory" is actually a file
                if (errno == EEXIST && dd_path_kind(fs, path) == DD_KIND_OTHER) {
                    errno = ENOTDIR;
                    break;
                }
            }

            *p = '/';
            errno = 0;
        }
    }

    // Create the final target directory
    if (errno == 0) {
        create_directory(fs, path, permissions, uid, gid);
    }

    return (errno == 0) ? 0 : -1;
}


////////////////////////////////////////////////////////////////////////////////


errno_t cmd_makedir(const char* _Nonnull dmgPath, bool shouldCreateParents, fs_perms_t dirPerms, uid_t uid, gid_t gid, const clap_string_array_t* _Nonnull paths)
{
    if (paths->count < 1) {
        return ERANGE;
    }

    dd_fs_t fs = dd_mount(dmgPath);
    if (fs == NULL) {
        return errno;
    }

    for (size_t i = 0; i < paths->count; i++) {
        const char* cur_path = paths->strings[i];
        int r;

        if (dd_validate_path(cur_path) < 0) {
            break;
        }

        if (shouldCreateParents) {
            r = create_directory_recursively(fs, cur_path, dirPerms, uid, gid);
        }
        else {
            r = create_directory(fs, cur_path, dirPerms, uid, gid);
        }

        if (r < 0) {
            break;
        }
    }

    dd_unmount(fs, (errno == 0) ? DD_USYNC : 0);
    return errno;
}
