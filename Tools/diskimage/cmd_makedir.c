//
//  cmd_makedir.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "DiskController.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static errno_t _create_directory(FileManagerRef _Nonnull fm, const char* _Nonnull path, FilePermissions perms, UserId uid, GroupId gid)
{
    decl_try_err();

    err = FileManager_CreateDirectory(fm, path, perms);
    if (err == EOK) {
        MutableFileInfo info;

        info.modify = kModifyFileInfo_UserId | kModifyFileInfo_GroupId;
        info.uid = uid;
        info.gid = gid;
        err = FileManager_SetFileInfo(fm, path, &info);

        if (err != EOK) {
            FileManager_Unlink(fm, path);
        }
    }

    return err;
}

static errno_t _create_directory_recursively(DiskControllerRef _Nonnull self, char* _Nonnull path, FilePermissions permissions, UserId uid, GroupId gid)
{
    decl_try_err();
    char* p = path;

    while (*p == '/') {
        p++;
    }

    do {
        char* ps = strchr(p, '/');

        if (ps) { *ps = '\0'; }

        err = _create_directory(&self->fm, path, permissions, uid, gid);
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
static errno_t create_directory_recursively(DiskControllerRef _Nonnull self, char* _Nonnull path, FilePermissions permissions, UserId uid, GroupId gid)
{
    decl_try_err();
    int i = 0;

    while (i < 16) {
        err = _create_directory_recursively(self, path, permissions, uid, gid);
        if (err == EOK || err != ENOENT) {
            break;
        }

        i++;
    }
    
    return err;
}


////////////////////////////////////////////////////////////////////////////////


errno_t cmd_makedir(bool shouldCreateParents, FilePermissions dirPerms, UserId uid, GroupId gid, const char* _Nonnull path_, const char* _Nonnull dmgPath)
{
    decl_try_err();
    DiskControllerRef self;
    char* path;

    try(DiskController_CreateWithContentsOfPath(dmgPath, &self));
    try_null(path, strdup(path_), ENOMEM);

    err = _create_directory(&self->fm, path, dirPerms, uid, gid);
    if (err != EOK) {
        if (err == ENOENT && shouldCreateParents) {
            err = create_directory_recursively(self, path, dirPerms, uid, gid);
        }
    }

    if (err == EOK) {
        err = DiskController_WriteToPath(self, dmgPath);
    }

catch:
    DiskController_Destroy(self);
    return err;
}
