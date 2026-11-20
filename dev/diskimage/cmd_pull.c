//
//  cmd_pull.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "path_utils.h"
#include "rdir.h"
#include "utils.h"
#include "FSManager.h"
#include <stdio.h>
#include <ext/errno.h>
#include <handler/Handler.h>

extern int dmg_path_kind(FSManagerRef fsm, const char *path);


/* Intermediate paths. Static, so neither reentrant nor thread-safe. */
static char g_cp_path[DMG_PATH_MAX];
static char g_cp_tmp[DMG_PATH_MAX];


static errno_t copy_file(FSManagerRef _Nonnull fsm, const char* _Nonnull src_path, const char* _Nonnull dst_path)
{
    decl_try_err();
    HandlerRef hnd = NULL;
    FILE* fp = NULL;

    try(FileManager_OpenFile(&fsm->fm, src_path, O_RDONLY, &hnd));
    try_null(fp, fopen(dst_path, "wb"), errno);

    while (true) {
        ssize_t nBytesRead;
        
        err = Handler_Read(hnd, g_copy_buf, sizeof(g_copy_buf), &nBytesRead);
        if (err != EOK || nBytesRead == 0) {
            break;
        }

        fwrite(g_copy_buf, 1, nBytesRead, fp);
        if (ferror(fp)) {
            err = errno;
            break;
        }
        else if (feof(fp)) {
            err = EIO;
            break;
        }
    }

catch:
    if (fp) {
        fclose(fp);
    }
    Object_Release(hnd);

    return err;
}


static errno_t copy_file_hierarchy(FSManagerRef _Nonnull fsm, const char * _Nonnull src_path, const char * _Nonnull dst_path)
{
    rdir_t it = NULL;
    const rdir_entry_t *e;
    int created;
    int result = -1;

    if (host_create_dir(dst_path, &created) != 0) {
        goto out;
    }


    it = rdir_open(fsm, src_path);
    if (it == NULL) {
        goto out;
    }

    while ((e = rdir_next(it)) != NULL) {
        int r;

        if (e->type == RDIR_TYPE_END_DIRECTORY) {
            continue;
        }

        g_cp_tmp[0] = '\0';
        host_path_append(g_cp_tmp, sizeof(g_cp_tmp), dst_path);
        host_path_append(g_cp_tmp, sizeof(g_cp_tmp), e->rel_path);
        const char *src = e->full_path;
        const char* dst = g_cp_tmp;

        switch (e->type) {
        case RDIR_TYPE_BEGIN_DIRECTORY:
            r = host_create_dir(dst, &created);
            break;

        case RDIR_TYPE_FILE:
            r = copy_file(fsm, src, dst);
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
        rdir_close(it);
        errno = saved;
    }
    return result;
}


static errno_t do_copy(FSManagerRef _Nonnull fsm, const char * _Nonnull src_path, const char * _Nonnull dst_path, bool bRecursive)
{
    const char *target = dst_path;
    int src_kind, dst_kind;

    if (src_path[0] == '\0' || dst_path[0] == '\0') {
        return ENOENT;
    }

    /* The source */
    src_kind = dmg_path_kind(fsm, src_path);
    if (src_kind < 0) {
        return errno;
    }
    if (src_kind == KIND_NONE) {
        return ENOENT;
    }
    if (dmg_ends_with_separator(src_path) && src_kind != KIND_DIRECTORY) {
        return ENOTDIR;            /* "file/" */
    }
    if (src_kind == KIND_DIRECTORY && !bRecursive) {
        return EISDIR;             /* a directory needs -R */
    }

    /* The destination */
    dst_kind = host_path_kind(dst_path);
    if (dst_kind < 0) {
        return errno;
    }
    if (dst_kind == KIND_DIRECTORY) {
        /* Copy into it: dst/<last component of src> */
        if (dmg_last_path_component(g_cp_tmp, sizeof(g_cp_tmp), src_path) < 0) {
            return errno;
        }
        /* Appended to an empty path, dst_path is kept as is (an absolute one stays absolute) */
        g_cp_path[0] = '\0';
        if (host_path_append(g_cp_path, sizeof(g_cp_path), dst_path) < 0
            || host_path_append(g_cp_path, sizeof(g_cp_path), g_cp_tmp) < 0) {
            return errno;
        }
        target = g_cp_path;
    }
    else if (host_ends_with_separator(dst_path)) {
        /* "dst/" must be an existing directory */
        return (dst_kind == KIND_NONE) ? ENOENT : ENOTDIR;
    }
    else if (dst_kind == KIND_OTHER && src_kind == KIND_DIRECTORY) {
        return ENOTDIR;            /* a directory can't replace a non-directory */
    }
    /* Otherwise 'dst_path' doesn't exist and is created, or is a file that is overwritten */


    if (src_kind == KIND_DIRECTORY) {
        return copy_file_hierarchy(fsm, src_path, target);
    }
    else {
        return copy_file(fsm, src_path, target);
    }
}

errno_t cmd_pull(const char* _Nonnull dmgPath, const clap_string_array_t* _Nonnull paths, bool bRecursive)
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
        try(do_copy(fsm, paths->strings[i], dst_path, bRecursive));
    }

catch:
    FSManager_Destroy(fsm);
    Object_Release(disk);
    return err;
}
