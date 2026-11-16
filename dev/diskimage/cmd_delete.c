//
//  cmd_delete.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "path_utils.h"
#include "rdir.h"
#include <kpi/file.h>


/* Returns true if the last component of 'path' is "." or "..". */
static int is_dot_name(const char *path)
{
    /* A longer component doesn't fit and fails with ENAMETOOLONG, so it can't be either. */
    char last[3];

    return host_last_path_component(last, sizeof(last), path) == 0
        && (strcmp(last, ".") == 0 || strcmp(last, "..") == 0);
}


errno_t delete_file_hierarchy(FSManagerRef fsm, const char *path, bool bRecursive)
{
    fs_attr_t attr, root_attr;
    rdir_t it = NULL;
    const rdir_entry_t *e;
    errno_t err;
    int result = -1;

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

    err = FileManager_GetAttributes(&fsm->fm, path, &attr);
    if (err != 0) {
        return err;
    }
    if (attr.file_type != FS_FTYPE_DIR) {
        return FileManager_Unlink(&fsm->fm, path, __FS_ULNK_FIL_ONLY); /* a file or a link: never followed */
    }

    err = FileManager_GetAttributes(&fsm->fm, "/", &root_attr);
    if (err == 0 && attr.fsid == root_attr.fsid && attr.ino == root_attr.ino) {
        /* don't allow deleting the root directory */
        return EBUSY;
    }

    if (!bRecursive) {
        return 0;
    }


    it = rdir_open(fsm, path);
    if (it == NULL) {
        return errno;
    }

    err = 0;
    while ((e = rdir_next(it)) != NULL) {
        if (e->type == RDIR_TYPE_BEGIN_DIRECTORY) {
            continue;   /* removed at its END entry, once its contents are gone */
        }
        /*
         * At END the directory is empty, and the iterator has already closed
         * it. Everything else is unlinked: files, links (never followed),
         * FIFOs, ...
         */
        err = FileManager_Unlink(&fsm->fm, e->full_path, __FS_ULNK_ANY);
        if (err != 0 && err != ENOENT) {
            goto out;
        }
    }
    if (err != 0) {
        goto out;       /* the iteration ended with an error */
    }

    rdir_close(it);   /* closes the root directory */
    it = NULL;
    return FileManager_Unlink(&fsm->fm, path, __FS_ULNK_DIR_ONLY);

out:
    rdir_close(it);
    return err;
}

errno_t cmd_delete(const char* _Nonnull path, bool bRecursive, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef fsm = NULL;

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &fsm));
    try(dmg_set_path(path));

    try(delete_file_hierarchy(fsm, g_dmg_path, bRecursive));

catch:
    FSManager_Destroy(fsm);
    if (err == EOK) {
        err = RamContainer_WriteToPath(disk, dmgPath);
    }
    Object_Release(disk);
    return err;
}
