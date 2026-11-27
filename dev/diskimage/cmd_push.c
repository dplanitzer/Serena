//
//  cmd_push.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/18/24.
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


static dd_fd_t create_file(dd_fs_t _Nonnull fs, const char* _Nonnull path, fs_perms_t fsperms, uid_t uid, gid_t gid)
{
    errno = 0;

    dd_fd_t fd = dd_open(fs, path, DD_WRONLY | DD_TRUNC);
    if ((fd == NULL) && (errno == ENOENT)) {
        errno = 0;
        fd = dd_create(fs, path, DD_WRONLY, fsperms);
    }

    if (fd) {
        // Update the inode metadata
        if ((dd_chown(fs, path, uid, gid) < 0) || (dd_chperm(fs, path, fsperms) < 0)) {
            dd_close(fd);
            dd_remove(fs, path);
            fd = NULL;
        }
    }

    return fd;
}

static int copy_file(dd_fs_t _Nonnull fs, fs_perms_t fsperms, uid_t uid, gid_t gid, const char* _Nonnull src_path, const char* _Nonnull dst_path)
{
    dd_fd_t dp = NULL;
    FILE* sp = NULL;

    sp = hd_fopen(src_path, "rb");
    if (sp == NULL) {
        return -1;
    }
    
    dp = create_file(fs, dst_path, fsperms, uid, gid);
    if (dp) {
        for (;;) {
            const size_t nBytesRead = fread(g_copy_buf, 1, sizeof(g_copy_buf), sp);

            if (nBytesRead == 0) {
                break;
            }

            if (dd_write_all(dp, g_copy_buf, nBytesRead) < 0) {
                break;
            }
        }

        dd_close(dp);
    }
    fclose(sp);

    return (errno == 0) ? 0 : -1;
}

static int create_directory(dd_fs_t _Nonnull fs, const char *path, int *created)
{
    if (!dd_makedir(fs, path, 0755)) {
        if (created) {
            *created = 1;
        }
        return 0;
    }
    if (errno != EEXIST) {
        return -1;
    }

    switch (dd_path_kind(fs, path)) {
        case DD_KIND_ERROR:
            return -1;

        case DD_KIND_OTHER:
            errno = ENOTDIR;
            return -1;
    }
    if (created) {
        *created = 0;
    }

    return 0;
}


static int copy_file_hierarchy(dd_fs_t _Nonnull fs, fs_perms_t fsperms, uid_t uid, gid_t gid, const char * _Nonnull src_path, const char * _Nonnull dst_path)
{
    hd_rdir_t it;
    const hd_rdir_entry_t *e;

    if (create_directory(fs, dst_path, NULL) < 0) {
        return -1;
    }


    it = hd_rdir_open(src_path);
    if (it == NULL) {
        return -1;
    }

    while ((e = hd_rdir_next(it)) != NULL) {
        if (e->type == HD_RDIR_TYPE_END_DIRECTORY) {
            continue;
        }

        g_cp_tmp[0] = '\0';
        dd_path_append(g_cp_tmp, sizeof(g_cp_tmp), dst_path);
        dd_path_append(g_cp_tmp, sizeof(g_cp_tmp), e->rel_path);
        const char *src = e->full_path;
        const char* dst = g_cp_tmp;
        int r;

        switch (e->type) {
        case HD_RDIR_TYPE_BEGIN_DIRECTORY:
            r = create_directory(fs, dst, NULL);
            break;

        case HD_RDIR_TYPE_FILE:
            r = copy_file(fs, fsperms, uid, gid, src, dst);
            break;

        default:
            break;
        }

        if (r < 0) {
            break;
        }
    }

    const int saved = errno;
    hd_rdir_close(it);
    errno = saved;

    return (errno == 0) ? 0 : -1;
}


static errno_t do_copy(dd_fs_t _Nonnull fs, fs_perms_t fsperms, uid_t uid, gid_t gid, const char * _Nonnull src_path, const char * _Nonnull dst_path, bool bRecursive)
{
    const char *target = dst_path;
    int src_kind, dst_kind;

    if (src_path[0] == '\0' || dst_path[0] == '\0') {
        return ENOENT;
    }

    /* The source */
    src_kind = hd_path_kind(src_path);
    if (src_kind < 0) {
        return errno;
    }
    if (src_kind == HD_KIND_NONE) {
        return ENOENT;
    }
    if (hd_ends_with_separator(src_path) && src_kind != HD_KIND_DIRECTORY) {
        return ENOTDIR;            /* "file/" */
    }
    if (src_kind == HD_KIND_DIRECTORY && !bRecursive) {
        return EISDIR;             /* a directory needs -R */
    }

    /* The destination */
    dst_kind = dd_path_kind(fs, dst_path);
    if (dst_kind < 0) {
        return errno;
    }
    if (dst_kind == DD_KIND_DIRECTORY) {
        /* Copy into it: dst/<last component of src> */
        if (hd_last_path_component(g_cp_tmp, sizeof(g_cp_tmp), src_path) < 0) {
            return errno;
        }
        /* Appended to an empty path, dst_path is kept as is (an absolute one stays absolute) */
        g_cp_path[0] = '\0';
        if (dd_path_append(g_cp_path, sizeof(g_cp_path), dst_path) < 0
            || dd_path_append(g_cp_path, sizeof(g_cp_path), g_cp_tmp) < 0) {
            return errno;
        }
        target = g_cp_path;
    }
    else if (dd_ends_with_separator(dst_path)) {
        /* "dst/" must be an existing directory */
        return (dst_kind == DD_KIND_NONE) ? ENOENT : ENOTDIR;
    }
    else if (dst_kind == DD_KIND_OTHER && src_kind == HD_KIND_DIRECTORY) {
        return ENOTDIR;            /* a directory can't replace a non-directory */
    }
    /* Otherwise 'dst_path' doesn't exist and is created, or is a file that is overwritten */


    if (src_kind == HD_KIND_DIRECTORY) {
        if (copy_file_hierarchy(fs, fsperms, uid, gid, src_path, target) < 0) {
            return errno;
        }
    }
    else {
        if (copy_file(fs, fsperms, uid, gid, src_path, target) < 0) {
            return errno;
        }
    }

    return 0;
}


errno_t cmd_push(const char* _Nonnull dmgPath, fs_perms_t fsperms, uid_t uid, gid_t gid, const clap_string_array_t* _Nonnull paths, bool bRecursive)
{
    decl_try_err();
    dd_fs_t fs = NULL;

    if (paths->count < 2) {
        return ERANGE;
    }

    try_null(fs, dd_mount(dmgPath), errno);

    const char* dst_path = paths->strings[paths->count - 1];
    for (size_t i = 0; i < paths->count - 1; i++) {
        const char* src_path = paths->strings[i];

        try(hd_validate_path(src_path));
        try(do_copy(fs, fsperms, uid, gid, src_path, dst_path, bRecursive));
    }

catch:
    dd_unmount(fs, (err == 0) ? DD_USYNC : 0);
    return err;
}
