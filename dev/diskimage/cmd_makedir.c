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
#include <ext/perm.h>
#include <kpi/fcntl.h>


static errno_t _create_directory(FileManagerRef _Nonnull fm, const char* _Nonnull path, mode_t perms, uid_t uid, gid_t gid)
{
    decl_try_err();

    err = FileManager_CreateDirectory(fm, path, perms);
    if (err == EOK) {
        err = FileManager_SetFileOwner(fm, path, uid, gid);

        if (err != EOK) {
            FileManager_Unlink(fm, path, __ULNK_ANY);
        }
    }

    return err;
}

static errno_t _create_directory_recursively(FileManagerRef fm, char* _Nonnull path, mode_t permissions, uid_t uid, gid_t gid)
{
    decl_try_err();
    char* p = path;

    while (*p == '/') {
        p++;
    }

    do {
        char* ps = strchr(p, '/');

        if (ps) { *ps = '\0'; }

        err = _create_directory(fm, path, permissions, uid, gid);
        if (ps) { *ps = '/'; }

        if (err != EOK && err != EEXIST || ps == NULL) {
            break;
        }

        p = ps;
        while (*p == '/') {
            p++;
        }
    } while(*p != '\0');

    return err;
}

// Iterate the path components from the root on down and try creating the
// corresponding directory. If it fails with EEXIST then we know that this
// directory already exists. Any other error is treated as fatal. If it worked
// then continue until we hit the end of the path.
// Note that we may find ourselves racing with another process that is busy
// deleting one of the path components we thought existed. Ie we try to do
// a create directory on path component X that comes back with EEXIST. We now
// move on to the child X/Y and try the create directory there. However this
// may now come back with ENOENT because X was empty and it got deleted by
// another process. We simply start over again from the root of our path in
// this case.
static errno_t create_directory_recursively(FileManagerRef _Nonnull fm, char* _Nonnull path, mode_t permissions, uid_t uid, gid_t gid)
{
    decl_try_err();
    int i = 0;

    while (i < 16) {
        err = _create_directory_recursively(fm, path, permissions, uid, gid);
        if (err == EOK || err != ENOENT) {
            break;
        }

        i++;
    }
    
    return err;
}


////////////////////////////////////////////////////////////////////////////////


errno_t cmd_makedir(bool shouldCreateParents, mode_t dirPerms, uid_t uid, gid_t gid, const char* _Nonnull path_, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef m = NULL;
    char* path;

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &m));

    try_null(path, strdup(path_), ENOMEM);

    err = _create_directory(&m->fm, path, dirPerms, uid, gid);
    if (err != EOK) {
        if (err == ENOENT && shouldCreateParents) {
            err = create_directory_recursively(&m->fm, path, dirPerms, uid, gid);
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
