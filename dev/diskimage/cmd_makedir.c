//
//  cmd_makedir.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "FSManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kpi/file.h>
#include <kpi/fs_perms.h>

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

static errno_t create_directory_recursively(FileManagerRef fm, char* _Nonnull path, fs_perms_t permissions, uid_t uid, gid_t gid)
{
    decl_try_err();
    di_path_iter_t it = DI_PATH_ITER_INITIALIZER;
    fs_attr_t attr;

    // Iterate through the path string
    while (!path_next(&it, DMG_PATH, path)) {
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
        }
    }

    // Create the final target directory
    if (err == EOK) {
        err = create_directory(fm, path, permissions, uid, gid);
    }

    return err;
}


////////////////////////////////////////////////////////////////////////////////


errno_t cmd_makedir(bool shouldCreateParents, fs_perms_t dirPerms, uid_t uid, gid_t gid, const char* _Nonnull dirPath, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef m = NULL;

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &m));

    try(set_dmg_path(dirPath));

    err = create_directory(&m->fm, g_dmg_path, dirPerms, uid, gid);
    if (err != EOK) {
        if (err == ENOENT && shouldCreateParents) {
            err = create_directory_recursively(&m->fm, g_dmg_path, dirPerms, uid, gid);
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
