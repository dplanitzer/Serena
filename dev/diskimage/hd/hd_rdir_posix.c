//
//  hd_rdir_posix.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE     /* glibc: expose d_type / DT_* */
#define _DARWIN_C_SOURCE    /* macOS: expose d_type / DT_* */

#include "hd_rdir.h"
#include "path_utils.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* One open directory on the descent stack. */
typedef struct hd_rdir_frame {
    DIR    *dir;
    size_t  path_len;   /* length of this directory's full path in 'path' */
    size_t  name_off;   /* offset of the directory's name in 'path' */
} hd_rdir_frame;

struct hdir {
    hd_rdir_frame     *stack;
    size_t          depth;
    size_t          stack_cap;

    int             error;          /* errno of the error that ended the iteration, or 0 */
    int             done;           /* the iteration is complete */

    size_t          root_len;       /* length of the root's path in 'path' */
    size_t          rel_off;        /* where relative paths start in 'path' */

    hd_rdir_entry_t    entry;          /* returned by hd_rdir_next() */
    char            path[HD_RDIR_PATH_MAX];        /* full path of the current entry */
};


static DIR *open_dir_at(int dir_fd, const char *name, int extra_flags)
{
    int fd = openat(dir_fd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC | extra_flags);
    if (fd < 0) {
        return NULL;
    }

    DIR *d = fdopendir(fd);
    if (d == NULL) {
        int saved = errno;
        close(fd);
        errno = saved;
    }
    return d;
}

static int push_frame(hd_rdir_t it, DIR *d, size_t path_len, size_t name_off)
{
    if (it->depth == it->stack_cap) {
        size_t new_cap = it->stack_cap ? it->stack_cap * 2 : 16;
        hd_rdir_frame *s = realloc(it->stack, new_cap * sizeof(*s));
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

static void pop_frame(hd_rdir_t it)
{
    it->depth--;
    closedir(it->stack[it->depth].dir);
}

/*
 * Ends the iteration with 'err', releasing the open subdirectories right
 * away. The root stays open for hd_rdir_rewind().
 */
static const hd_rdir_entry_t *fail(hd_rdir_t it, int err)
{
    while (it->depth > 1) {
        pop_frame(it);
    }
    it->error = err;
    errno = err;
    return NULL;
}

static const hd_rdir_entry_t *set_entry(hd_rdir_t it, size_t name_off, hd_rdir_type_t type, int depth)
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
 * hd_path_append() decides whether a separator goes between the root and
 * a name (none after a bare Windows drive "C:"), so let it append one.
 */
static int set_root_path(hd_rdir_t it, const char *root)
{
    size_t len = strlen(root);

    if (len + 1 > sizeof(it->path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(it->path, root, len + 1);
    it->root_len = len;

    errno = 0;
    if (hd_path_append(it->path, sizeof(it->path), "x") < 0) {
        if (errno == 0) {
            errno = ENAMETOOLONG;
        }
        return -1;
    }
    it->rel_off = strlen(it->path) - 1;
    it->path[len] = '\0';
    return 0;
}

static hd_rdir_type_t entry_type(DIR *parent, const struct dirent *ent)
{
#if defined(DT_DIR) && defined(DT_UNKNOWN)
    switch (ent->d_type) {
    case DT_DIR:     return HD_RDIR_TYPE_BEGIN_DIRECTORY;
    case DT_REG:     return HD_RDIR_TYPE_FILE;
    case DT_LNK:     return HD_RDIR_TYPE_SYMLINK;
    case DT_UNKNOWN: break;
    default:         return HD_RDIR_TYPE_OTHER;
    }
#endif
    struct stat st;
    if (fstatat(dirfd(parent), ent->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
        return HD_RDIR_TYPE_OTHER;     /* vanished or inaccessible: report it, but don't descend */
    }
    if (S_ISDIR(st.st_mode)) {
        return HD_RDIR_TYPE_BEGIN_DIRECTORY;
    }
    if (S_ISREG(st.st_mode)) {
        return HD_RDIR_TYPE_FILE;
    }
    if (S_ISLNK(st.st_mode)) {
        return HD_RDIR_TYPE_SYMLINK;
    }
    return HD_RDIR_TYPE_OTHER;
}


hd_rdir_t hd_rdir_open(const char *root_path)
{
    if (root_path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    hd_rdir_t it = calloc(1, sizeof(*it));
    if (it == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    if (set_root_path(it, root_path) != 0) {
        goto fail;
    }

    /* The root itself may be a symlink; only links inside the tree are not followed. */
    DIR *root = open_dir_at(AT_FDCWD, root_path, 0);
    if (root == NULL) {
        goto fail;
    }
    if (push_frame(it, root, it->root_len, it->root_len) != 0) {
        closedir(root);
        errno = ENOMEM;
        goto fail;
    }
    return it;

fail:
    {
        int saved = errno;
        hd_rdir_close(it);
        errno = saved;
    }
    return NULL;
}

void hd_rdir_close(hd_rdir_t it)
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

void hd_rdir_rewind(hd_rdir_t it)
{
    /* The root is always open, and rewinddir() can't fail. */
    while (it->depth > 1) {
        pop_frame(it);
    }
    rewinddir(it->stack[0].dir);
    it->path[it->root_len] = '\0';
    it->error = 0;
    it->done = 0;
}

const hd_rdir_entry_t *hd_rdir_next(hd_rdir_t it)
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
        hd_rdir_frame *top = &it->stack[it->depth - 1];

        errno = 0;
        struct dirent *ent = readdir(top->dir);
        if (ent == NULL) {
            if (errno != 0) {
                return fail(it, errno);
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
            return set_entry(it, name_off, HD_RDIR_TYPE_END_DIRECTORY, (int)level);
        }

        const char *name = ent->d_name;
        if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
            continue;
        }

        /* Replace the previous entry's name with this one. */
        it->path[top->path_len] = '\0';
        errno = 0;
        if (hd_path_append(it->path, sizeof(it->path), name) < 0) {
            return fail(it, (errno != 0) ? errno : ENAMETOOLONG);
        }
        size_t path_len = strlen(it->path);
        size_t name_off = path_len - strlen(name);
        int level = (int)it->depth;

        hd_rdir_type_t type = entry_type(top->dir, ent);
        if (type == HD_RDIR_TYPE_BEGIN_DIRECTORY) {
            /* Entered right away, so BEGIN is only reported for directories that can be read. */
            DIR *d = open_dir_at(dirfd(top->dir), name, O_NOFOLLOW);
            if (d == NULL) {
                return fail(it, errno);
            }
            if (push_frame(it, d, path_len, name_off) != 0) {
                closedir(d);
                return fail(it, ENOMEM);
            }
        }
        return set_entry(it, name_off, type, level);
    }

    errno = 0;
    return NULL;
}

#endif  /* __POSIX__ */
