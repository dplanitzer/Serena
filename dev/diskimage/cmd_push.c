//
//  cmd_push.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "FSManager.h"
#include <ext/errno.h>
#include <handler/Handler.h>
#include <kpi/file.h>
#include <kpi/fs_perms.h>


static errno_t _create_file(FileManagerRef _Nonnull fm, const char* _Nonnull path, fs_perms_t fsperms, uid_t uid, gid_t gid, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    decl_try_err();
    HandlerRef hnd = NULL;

    err = FileManager_OpenFile(fm, path, O_WRONLY | O_TRUNC, &hnd);
    if (err == ENOENT) {
        // File doesn't exist yet. Create it
        err = FileManager_CreateFile(fm, path, O_WRONLY, fsperms, &hnd);
    }

    if (err == EOK) {
        // Update the inode metadata
        //XXX use handler versions once they exist
        err = FileManager_SetFileOwner(fm, path, uid, gid);
        if (err == EOK) {
            err = FileManager_SetFilePermissions(fm, path, fsperms);
        }

        if (err != EOK) {
            FileManager_Unlink(fm, path, __FS_ULNK_ANY);
        }
    }

    *pOutHandler = hnd;
    return err;
}

errno_t cmd_push(fs_perms_t fsperms, uid_t uid, gid_t gid, const char* _Nonnull srcPath, const char* _Nonnull dstPath, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef m = NULL;
    HandlerRef dfp = NULL;
    FILE* sfp = NULL;

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &m));

    try_null(sfp, fopen(srcPath, "rb"), errno);
    try(set_dmg_path(dstPath));

    const errno_t r = _create_file(&m->fm, g_dmg_path, fsperms, uid, gid, &dfp);
    if (r == EISDIR) {
        if (last_path_component(HOST_PATH, g_last_path_comp, sizeof(g_last_path_comp), srcPath) < 0) {
            throw(errno);
        }
        if (path_append(DMG_PATH, g_dmg_path, sizeof(g_dmg_path), g_last_path_comp) < 0) {
            throw(errno);
        }

        try(_create_file(&m->fm, g_dmg_path, fsperms, uid, gid, &dfp));
    }
    else if (r != EOK) {
        throw(r);
    }

    while (true) {
        const size_t nBytesRead = fread(g_copy_buf, 1, sizeof(g_copy_buf), sfp);
        if (nBytesRead == 0) {
            if (ferror(sfp)) {
                err = errno;
            }
            break;
        }

        ssize_t nBytesWritten;
        err = Handler_Write(dfp, g_copy_buf, nBytesRead, &nBytesWritten);
        if (err != EOK) {
            break;
        }
    }

catch:
    Object_Release(dfp);
    if (sfp) {
        fclose(sfp);
    }

    FSManager_Destroy(m);
    if (err == EOK) {
        err = RamContainer_WriteToPath(disk, dmgPath);
    }
    Object_Release(disk);
    return err;
}
