//
//  cmd_push.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "path_utils.h"
#include "hdir.h"
#include "utils.h"
#include "FSManager.h"
#include <stdio.h>
#include <ext/errno.h>
#include <handler/Handler.h>
#include <kpi/file.h>
#include <kpi/fs_perms.h>

extern int dmg_path_kind(FSManagerRef fsm, const char *path);


/* Intermediate paths. Static, so neither reentrant nor thread-safe. */
static char g_cp_path[DMG_PATH_MAX];
static char g_cp_tmp[DMG_PATH_MAX];


static errno_t dmg_create_file(FileManagerRef _Nonnull fm, const char* _Nonnull path, fs_perms_t fsperms, uid_t uid, gid_t gid, HandlerRef _Nullable * _Nonnull pOutHandler)
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

static errno_t copy_file(FSManagerRef _Nonnull fsm, fs_perms_t fsperms, uid_t uid, gid_t gid, const char* _Nonnull src_path, const char* _Nonnull dst_path)
{
    decl_try_err();
    HandlerRef dfp = NULL;
    FILE* sfp = NULL;

    try_null(sfp, fopen(src_path, "rb"), errno);
    try(dmg_create_file(&fsm->fm, dst_path, fsperms, uid, gid, &dfp));

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

    return err;
}

static int dmg_create_dir(FSManagerRef fsm, const char *path, int *created)
{
    errno_t err;

    err = FileManager_CreateDirectory(&fsm->fm, path, 0755);
    if (err == 0) {
        *created = 1;
        return 0;
    }
    if (err != EEXIST) {
        errno = err;
        return -1;
    }

    fs_attr_t attr;
    err = FileManager_GetAttributes(&fsm->fm, path, &attr);
    if (err != 0) {
        errno = err;
        return -1;
    }
    if (attr.file_type != FS_FTYPE_DIR) {
        errno = ENOTDIR;
        return -1;
    }
    *created = 0;

    return 0;
}


static errno_t copy_file_hierarchy(FSManagerRef _Nonnull fsm, fs_perms_t fsperms, uid_t uid, gid_t gid, const char * _Nonnull src_path, const char * _Nonnull dst_path)
{
    hdir_t it = NULL;
    const hdir_entry_t *e;
    int created;
    int result = -1;

    if (dmg_create_dir(fsm, dst_path, &created) != 0) {
        goto out;
    }


    it = hdir_open(src_path);
    if (it == NULL) {
        goto out;
    }

    while ((e = hdir_next(it)) != NULL) {
        int r;

        if (e->type == HDIR_TYPE_END_DIRECTORY) {
            continue;
        }

        g_cp_tmp[0] = '\0';
        dmg_path_append(g_cp_tmp, sizeof(g_cp_tmp), dst_path);
        dmg_path_append(g_cp_tmp, sizeof(g_cp_tmp), e->rel_path);
        const char *src = e->full_path;
        const char* dst = g_cp_tmp;

        switch (e->type) {
        case HDIR_TYPE_BEGIN_DIRECTORY:
            r = dmg_create_dir(fsm, dst, &created);
            break;

        case HDIR_TYPE_FILE:
            r = copy_file(fsm, fsperms, uid, gid, src, dst);
            break;

        default:
            break;
        }

        if (r != 0) {
            errno = r;
            goto out;
        }
    }
    if (errno != 0) {
        goto out;       /* the iteration ended with an error */
    }
    result = 0;

out:
    {
        int saved = errno;
        hdir_close(it);
        errno = saved;
    }
    return result;
}


static errno_t do_copy(FSManagerRef _Nonnull fsm, fs_perms_t fsperms, uid_t uid, gid_t gid, const char * _Nonnull src_path, const char * _Nonnull dst_path, bool bRecursive)
{
    const char *target = dst_path;
    int src_kind, dst_kind;

    if (src_path[0] == '\0' || dst_path[0] == '\0') {
        return ENOENT;
    }

    /* The source */
    src_kind = host_path_kind(src_path);
    if (src_kind < 0) {
        return errno;
    }
    if (src_kind == KIND_NONE) {
        return ENOENT;
    }
    if (host_ends_with_separator(src_path) && src_kind != KIND_DIRECTORY) {
        return ENOTDIR;            /* "file/" */
    }
    if (src_kind == KIND_DIRECTORY && !bRecursive) {
        return EISDIR;             /* a directory needs -R */
    }

    /* The destination */
    dst_kind = dmg_path_kind(fsm, dst_path);
    if (dst_kind < 0) {
        return errno;
    }
    if (dst_kind == KIND_DIRECTORY) {
        /* Copy into it: dst/<last component of src> */
        if (host_last_path_component(g_cp_tmp, sizeof(g_cp_tmp), src_path) < 0) {
            return errno;
        }
        /* Appended to an empty path, dst_path is kept as is (an absolute one stays absolute) */
        g_cp_path[0] = '\0';
        if (dmg_path_append(g_cp_path, sizeof(g_cp_path), dst_path) < 0
            || dmg_path_append(g_cp_path, sizeof(g_cp_path), g_cp_tmp) < 0) {
            return errno;
        }
        target = g_cp_path;
    }
    else if (dmg_ends_with_separator(dst_path)) {
        /* "dst/" must be an existing directory */
        return (dst_kind == KIND_NONE) ? ENOENT : ENOTDIR;
    }
    else if (dst_kind == KIND_OTHER && src_kind == KIND_DIRECTORY) {
        return ENOTDIR;            /* a directory can't replace a non-directory */
    }
    /* Otherwise 'dst_path' doesn't exist and is created, or is a file that is overwritten */


    if (src_kind == KIND_DIRECTORY) {
        return copy_file_hierarchy(fsm, fsperms, uid, gid, src_path, target);
    }
    else {
        return copy_file(fsm, fsperms, uid, gid, src_path, target);
    }
}


errno_t cmd_push(const char* _Nonnull dmgPath, fs_perms_t fsperms, uid_t uid, gid_t gid, const clap_string_array_t* _Nonnull paths, bool bRecursive)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef fsm = NULL;

    if (paths->count < 2) {
        return ERANGE;
    }

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &fsm));

    const char* dst_path = paths->strings[paths->count - 1];
    for (size_t i = 0; i < paths->count - 1; i++) {
        try(do_copy(fsm, fsperms, uid, gid, paths->strings[i], dst_path, bRecursive));
    }

catch:
    FSManager_Destroy(fsm);
    if (err == EOK) {
        err = RamContainer_WriteToPath(disk, dmgPath);
    }
    Object_Release(disk);
    return err;
}
