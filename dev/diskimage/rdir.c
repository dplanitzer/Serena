//
//  rdir.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "rdir.h"
#include "path_utils.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <handler/Handler.h>

/* One open directory on the descent stack. */
typedef struct rdir_frame {
    HandlerRef  dir;
    size_t      path_len;   /* length of this directory's full path in 'path' */
    size_t      name_off;   /* offset of the directory's name in 'path' */
} rdir_frame;

struct rdir {
    FSManagerRef    fsm;
    rdir_frame     *stack;
    size_t          depth;
    size_t          stack_cap;

    int             error;          /* errno of the error that ended the iteration, or 0 */
    int             done;           /* the iteration is complete */

    size_t          root_len;       /* length of the root's path in 'path' */
    size_t          rel_off;        /* where relative paths start in 'path' */

    dir_entry_t     dir_entry;      /* current FSManager directory entry */
    rdir_entry_t    entry;          /* returned by rdir_next() */
    char            path[DMG_PATH_MAX];        /* full path of the current entry */
};

static int push_frame(rdir_t it, HandlerRef d, size_t path_len, size_t name_off)
{
    if (it->depth == it->stack_cap) {
        size_t new_cap = it->stack_cap ? it->stack_cap * 2 : 16;
        rdir_frame *s = realloc(it->stack, new_cap * sizeof(*s));
        if (s == NULL) {
            errno = ENOMEM;
            return -1;
        }
        it->stack = s;
        it->stack_cap = new_cap;
    }

    it->stack[it->depth].dir = d;
    it->stack[it->depth].path_len = path_len;
    it->stack[it->depth].name_off = name_off;
    it->depth++;
    return 0;
}

static void pop_frame(rdir_t it)
{
    it->depth--;
    Object_Release(it->stack[it->depth].dir);
}

/*
 * Ends the iteration with 'err', releasing the open subdirectories right
 * away. The root stays open for rdir_rewind().
 */
static const rdir_entry_t *fail(rdir_t it, int err)
{
    while (it->depth > 1) {
        pop_frame(it);
    }
    it->error = err;
    errno = err;
    return NULL;
}

static const rdir_entry_t *set_entry(rdir_t it, size_t name_off, rdir_type_t type, int depth)
{
    it->entry.full_path = it->path;
    it->entry.rel_path = it->path + it->rel_off;
    it->entry.name = it->path + name_off;
    it->entry.type = type;
    it->entry.depth = depth;
    return &it->entry;
}

/*
 * Puts the root into 'path' and finds where relative paths will start.
 */
static int set_root_path(rdir_t it, const char *root)
{
    size_t len = strlen(root);

    if (len + 1 > sizeof(it->path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(it->path, root, len + 1);
    it->root_len = len;

    errno = 0;
    if (dmg_path_append(it->path, sizeof(it->path), "x") < 0) {
        if (errno == 0) {
            errno = ENAMETOOLONG;
        }
        return -1;
    }
    it->rel_off = strlen(it->path) - 1;
    it->path[len] = '\0';
    return 0;
}

static rdir_type_t entry_type(rdir_t it, HandlerRef parent)
{
    fs_attr_t attr;
    const errno_t err = FileManager_GetAttributes(&it->fsm->fm, it->path, &attr);
    if (err != 0) {
        return RDIR_TYPE_OTHER;     /* vanished or inaccessible: report it, but don't descend */
    }

    switch (attr.file_type) {
        case FS_FTYPE_REG:      return RDIR_TYPE_FILE;
        case FS_FTYPE_DIR:      return RDIR_TYPE_BEGIN_DIRECTORY;
        case FS_FTYPE_LNK:      return RDIR_TYPE_SYMLINK;
        default:                return RDIR_TYPE_OTHER;
    }
}


rdir_t rdir_open(FSManagerRef fsm, const char *root_path)
{
    if (fsm == NULL || root_path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    rdir_t it = calloc(1, sizeof(*it));
    if (it == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    it->fsm = fsm;
    if (set_root_path(it, root_path) != 0) {
        goto fail;
    }

    /* The root itself may be a symlink; only links inside the tree are not followed. */
    HandlerRef root = NULL;
    const errno_t err = FileManager_OpenDirectory(&fsm->fm, root_path, &root);
    if (err != 0) {
        errno = err;
        goto fail;
    }
    if (push_frame(it, root, it->root_len, it->root_len) != 0) {
        Object_Release(root);
        errno = ENOMEM;
        goto fail;
    }
    return it;

fail:
    {
        int saved = errno;
        rdir_close(it);
        errno = saved;
    }
    return NULL;
}

void rdir_close(rdir_t it)
{
    if (it == NULL) {
        return;
    }

    while (it->depth > 0) {
        pop_frame(it);
    }
    free(it->stack);
    free(it);
}

const rdir_entry_t *rdir_next(rdir_t it)
{
    if (it == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (it->error != 0) {
        errno = it->error;
        return NULL;
    }
    if (it->done) {
        errno = 0;
        return NULL;
    }

    while (it->depth > 0) {
        rdir_frame *top = &it->stack[it->depth - 1];
        ssize_t nBytesRead;

        const errno_t err = Handler_Read(top->dir, &it->dir_entry, sizeof(it->dir_entry), &nBytesRead);
        if ((err != 0) || (err == 0 && nBytesRead == 0)) {
            if (err != 0) {
                return fail(it, err);
            }

            /* The directory is finished. The stack index is its entries' depth - 1. */
            size_t level = it->depth - 1;
            if (level == 0) {
                it->done = 1;   /* the root: the iteration is complete; it stays open */
                break;
            }
            size_t path_len = top->path_len;
            size_t name_off = top->name_off;
            pop_frame(it);
            it->path[path_len] = '\0';
            return set_entry(it, name_off, RDIR_TYPE_END_DIRECTORY, (int)level);
        }

        const char *name = it->dir_entry.name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        /* Replace the previous entry's name with this one. */
        it->path[top->path_len] = '\0';
        errno = 0;
        if (dmg_path_append(it->path, sizeof(it->path), name) < 0) {
            return fail(it, (errno != 0) ? errno : ENAMETOOLONG);
        }
        size_t path_len = strlen(it->path);
        size_t name_off = path_len - strlen(name);
        int level = (int)it->depth;

        rdir_type_t type = entry_type(it, top->dir);
        if (type == RDIR_TYPE_BEGIN_DIRECTORY) {
            /* Entered right away, so BEGIN is only reported for directories that can be read. */
            HandlerRef d = NULL;
            const errno_t err = FileManager_OpenDirectory(&it->fsm->fm, it->path, &d);
            if (err != 0) {
                return fail(it, err);
            }
            if (push_frame(it, d, path_len, name_off) != 0) {
                Object_Release(d);
                return fail(it, ENOMEM);
            }
        }
        return set_entry(it, name_off, type, level);
    }

    errno = 0;
    return NULL;
}
