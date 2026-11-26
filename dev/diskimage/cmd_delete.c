//
//  cmd_delete.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <dd/dd.h>
#include <hd/hd.h>


/* Returns true if the last component of 'path' is "." or "..". */
static int is_dot_name(const char *path)
{
    /* A longer component doesn't fit and fails with ENAMETOOLONG, so it can't be either. */
    char last[3];

    return hd_last_path_component(last, sizeof(last), path) == 0
        && (strcmp(last, ".") == 0 || strcmp(last, "..") == 0);
}


errno_t delete_file_hierarchy(FSManagerRef fsm, const char *path, bool bRecursive)
{
    fs_attr_t attr, root_attr;
    dd_rdir_t it = NULL;
    const dd_rdir_entry_t *e;

    if (path == NULL) {
        return EINVAL;
    }
    if (path[0] == '\0') {
        return ENOENT;     /* checked first: the last component of "" is "." */
    }
    /* Like rm, refuse "." and "..": the walk would empty a directory that can't be removed. */
    if (is_dot_name(path)) {
        return EINVAL;
    }

    if (dd_attr(fsm, path, &attr) < 0) {
        return errno;
    }
    if (attr.file_type != FS_FTYPE_DIR) {
        /* a file or a link: never followed */
        if (dd_remove(fsm, path) < 0) {
            return errno;
        } else {
            return 0;
        }
    }

    if (dd_attr(fsm, "/", &root_attr) < 0) {
        return errno;
    }
    if (attr.fsid == root_attr.fsid && attr.ino == root_attr.ino) {
        /* don't allow deleting the root directory */
        return EBUSY;
    }

    if (!bRecursive) {
        /* complain that the user wants to delete a directory without the -r switch */
        errno = EISDIR;
        return errno;
    }


    it = dd_rdir_open(fsm, path);
    if (it == NULL) {
        return errno;
    }

    errno = 0;
    while ((e = dd_rdir_next(it)) != NULL) {
        if (e->type == DD_RDIR_TYPE_BEGIN_DIRECTORY) {
            continue;   /* removed at its END entry, once its contents are gone */
        }
        /*
         * At END the directory is empty, and the iterator has already closed
         * it. Everything else is unlinked: files, links (never followed),
         * FIFOs, ...
         */
        if (dd_remove(fsm, e->full_path) < 0 && errno != ENOENT) {
            break;
        }

        errno = 0;
    }

    const int saved = errno;
    dd_rdir_close(it);
    errno = saved;

    if (errno == 0) {
        dd_remove(fsm, path);
    }

    return errno;
}

errno_t cmd_delete(const char* _Nonnull dmgPath, bool bRecursive, const clap_string_array_t* _Nonnull paths)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef fsm = NULL;

    if (paths->count < 1) {
        return ERANGE;
    }

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &fsm));

    for (size_t i = 0; i < paths->count; i++) {
        const char* cur_path = paths->strings[i];

        try(dd_validate_path(cur_path));
        try(delete_file_hierarchy(fsm, cur_path, bRecursive));
    }

catch:
    FSManager_Destroy(fsm);
    if (err == EOK) {
        err = RamContainer_WriteToPath(disk, dmgPath);
    }
    Object_Release(disk);
    return err;
}
