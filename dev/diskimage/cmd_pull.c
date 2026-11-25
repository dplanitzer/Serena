//
//  cmd_pull.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <dd/dd.h>
#include <hd/hd.h>

#define COPY_BUF_SIZE   4096
static char g_copy_buf[COPY_BUF_SIZE];


/* Intermediate paths. Static, so neither reentrant nor thread-safe. */
static char g_cp_path[DD_PATH_MAX];
static char g_cp_tmp[DD_PATH_MAX];


static int copy_file(FSManagerRef _Nonnull fsm, const char* _Nonnull src_path, const char* _Nonnull dst_path)
{
    dd_fd_t sp = NULL;
    FILE* dp = NULL;

    errno = 0;
    sp = dd_open(fsm, src_path, DD_RDONLY);
    if (sp == NULL) {
        return -1;
    }

    dp = hd_fopen(dst_path, "wb");
    if (dp) {
        for (;;) {
            const ssize_t nBytesRead = dd_read(sp, g_copy_buf, sizeof(g_copy_buf));
        
            if (nBytesRead <= 0) {
                break;
            }

            fwrite(g_copy_buf, 1, nBytesRead, dp);
            if (ferror(dp)) {
                break;
            }
        }

        fclose(dp);
    }
    dd_close(sp);

    return (errno == 0) ? 0 : -1;
}


static int copy_file_hierarchy(FSManagerRef _Nonnull fsm, const char * _Nonnull src_path, const char * _Nonnull dst_path)
{
    dd_rdir_t it;
    const dd_rdir_entry_t *e;

    if (hd_create_dir(dst_path, NULL) < 0) {
        return -1;
    }

    it = dd_rdir_open(fsm, src_path);
    if (it == NULL) {
        return -1;
    }

    while ((e = dd_rdir_next(it)) != NULL) {
        if (e->type == DD_RDIR_TYPE_END_DIRECTORY) {
            continue;
        }

        g_cp_tmp[0] = '\0';
        hd_path_append(g_cp_tmp, sizeof(g_cp_tmp), dst_path);
        hd_path_append(g_cp_tmp, sizeof(g_cp_tmp), e->rel_path);
        const char *src = e->full_path;
        const char* dst = g_cp_tmp;
        int r;

        switch (e->type) {
        case DD_RDIR_TYPE_BEGIN_DIRECTORY:
            r = hd_create_dir(dst, NULL);
            break;

        case DD_RDIR_TYPE_FILE:
            r = copy_file(fsm, src, dst);
            break;

        default:
            break;
        }

        if (r < 0) {
            break;
        }
    }

    int saved = errno;
    dd_rdir_close(it);
    errno = saved;
    
    return (errno == 0) ? 0 : -1;
}


static errno_t do_copy(FSManagerRef _Nonnull fsm, const char * _Nonnull src_path, const char * _Nonnull dst_path, bool bRecursive)
{
    const char *target = dst_path;
    int src_kind, dst_kind;

    if (src_path[0] == '\0' || dst_path[0] == '\0') {
        return ENOENT;
    }

    /* The source */
    src_kind = dd_path_kind(fsm, src_path);
    if (src_kind < 0) {
        return errno;
    }
    if (src_kind == DD_KIND_NONE) {
        return ENOENT;
    }
    if (dd_ends_with_separator(src_path) && src_kind != DD_KIND_DIRECTORY) {
        return ENOTDIR;            /* "file/" */
    }
    if (src_kind == DD_KIND_DIRECTORY && !bRecursive) {
        return EISDIR;             /* a directory needs -R */
    }

    /* The destination */
    dst_kind = hd_path_kind(dst_path);
    if (dst_kind < 0) {
        return errno;
    }
    if (dst_kind == HD_KIND_DIRECTORY) {
        /* Copy into it: dst/<last component of src> */
        if (dd_last_path_component(g_cp_tmp, sizeof(g_cp_tmp), src_path) < 0) {
            return errno;
        }
        /* Appended to an empty path, dst_path is kept as is (an absolute one stays absolute) */
        g_cp_path[0] = '\0';
        if (hd_path_append(g_cp_path, sizeof(g_cp_path), dst_path) < 0
            || hd_path_append(g_cp_path, sizeof(g_cp_path), g_cp_tmp) < 0) {
            return errno;
        }
        target = g_cp_path;
    }
    else if (hd_ends_with_separator(dst_path)) {
        /* "dst/" must be an existing directory */
        return (dst_kind == HD_KIND_NONE) ? ENOENT : ENOTDIR;
    }
    else if (dst_kind == HD_KIND_OTHER && src_kind == DD_KIND_DIRECTORY) {
        return ENOTDIR;            /* a directory can't replace a non-directory */
    }
    /* Otherwise 'dst_path' doesn't exist and is created, or is a file that is overwritten */


    if (src_kind == DD_KIND_DIRECTORY) {
        if (copy_file_hierarchy(fsm, src_path, target) < 0) {
            return errno;
        }
    }
    else {
        if (copy_file(fsm, src_path, target) < 0) {
            return errno;
        }
    }

    return 0;
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
        const char* src_path = paths->strings[i];

        try(dd_validate_path(src_path));
        try(do_copy(fsm, src_path, dst_path, bRecursive));
    }

catch:
    FSManager_Destroy(fsm);
    Object_Release(disk);
    return err;
}
