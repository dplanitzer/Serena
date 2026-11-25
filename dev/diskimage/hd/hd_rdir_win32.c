//
//  hd_dir_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601     /* Windows 7: FindExInfoBasic, FIND_FIRST_EX_LARGE_FETCH */
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "hd_rdir.h"
#include "hd_path.h"
#include "utils_win32.h"

#include <windows.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

/*
 * The Win32 calls use the wide (UTF-16) API on a "\\?\" prefixed absolute
 * path, so MAX_PATH doesn't apply. The narrow path handed out is built
 * separately from the root as given, one converted name at a time.
 */

/* One open directory on the descent stack. */
typedef struct hd_rdir_frame {
    HANDLE           find;      /* INVALID_HANDLE_VALUE: the directory has no entries at all */
    size_t           wpath_len; /* length of this directory's full path in 'wpath' */
    size_t           path_len;  /* length of its full narrow path in 'path' */
    size_t           name_off;  /* offset of its name in 'path' */
    int              has_data;  /* 'data' holds an entry that was not processed yet */
    WIN32_FIND_DATAW data;
} hd_rdir_frame;

struct hdir {
    hd_rdir_frame     *stack;
    size_t          depth;
    size_t          stack_cap;

    wchar_t        *wpath;          /* "\\?\" + root + '\' + relative path, for the Win32 calls */
    size_t          wpath_len;
    size_t          wpath_cap;      /* in wchar_t */
    size_t          wroot_len;      /* length of the root's path in 'wpath' */

    UINT            codepage;       /* narrow encoding for root_path and returned paths */
    int             error;          /* errno of the error that ended the iteration, or 0 */
    int             restart;        /* hd_rdir_rewind() was called: search the root again */

    size_t          root_len;       /* length of the root's path in 'path' */
    size_t          rel_off;        /* where relative paths start in 'path' */

    hd_rdir_entry_t    entry;          /* returned by hd_rdir_next() */
    char            path[HD_RDIR_PATH_MAX];        /* full path of the current entry */
};


/* Ensures 'wpath' can hold 'need' wide characters (including NUL). */
static int reserve_wpath(hd_rdir_t it, size_t need)
{
    if (need <= it->wpath_cap) {
        return 0;
    }

    size_t new_cap = it->wpath_cap ? it->wpath_cap : 256;
    while (new_cap < need) {
        new_cap *= 2;
    }
    wchar_t *p = realloc(it->wpath, new_cap * sizeof(wchar_t));
    if (p == NULL) {
        errno = ENOMEM;
        return -1;
    }
    it->wpath = p;
    it->wpath_cap = new_cap;
    return 0;
}

/* Replaces everything after the directory path 'base_len' with '\' + 'name'. */
static int set_entry_wpath(hd_rdir_t it, size_t base_len, const wchar_t *name)
{
    size_t name_len = wcslen(name);

    if (reserve_wpath(it, base_len + 1 + name_len + 1) != 0) {
        return -1;
    }
    it->wpath[base_len] = L'\\';
    memcpy(it->wpath + base_len + 1, name, (name_len + 1) * sizeof(wchar_t));
    it->wpath_len = base_len + 1 + name_len;
    return 0;
}

/*
 * Starts enumerating the directory whose full path is wpath[0, wpath_len)
 * and pushes a frame for it.
 */
static int push_dir(hd_rdir_t it, size_t wpath_len, size_t path_len, size_t name_off)
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

    /* Temporarily turn "<dir>" into the search pattern "<dir>\*". */
    if (reserve_wpath(it, wpath_len + 3) != 0) {
        return -1;
    }
    it->wpath[wpath_len + 0] = L'\\';
    it->wpath[wpath_len + 1] = L'*';
    it->wpath[wpath_len + 2] = L'\0';

    hd_rdir_frame *f = &it->stack[it->depth];
    f->find = FindFirstFileExW(it->wpath, FindExInfoBasic, &f->data,
                               FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    DWORD err = GetLastError();
    it->wpath[wpath_len] = L'\0';

    if (f->find == INVALID_HANDLE_VALUE && err != ERROR_FILE_NOT_FOUND) {
        errno = win32_errno_from_error(err);
        return -1;
    }

    /* ERROR_FILE_NOT_FOUND: no entries at all, e.g. the root of an empty volume. */
    f->has_data = (f->find != INVALID_HANDLE_VALUE);
    f->wpath_len = wpath_len;
    f->path_len = path_len;
    f->name_off = name_off;
    it->depth++;
    return 0;
}

static void pop_frame(hd_rdir_t it)
{
    it->depth--;
    if (it->stack[it->depth].find != INVALID_HANDLE_VALUE) {
        FindClose(it->stack[it->depth].find);
    }
}

/* Ends the iteration with 'err', releasing the open directories right away. */
static const hd_rdir_entry_t *fail(hd_rdir_t it, int err)
{
    while (it->depth > 0) {
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

static hd_rdir_type_t entry_type(const WIN32_FIND_DATAW *fd)
{
    /* dwReserved0 holds the reparse tag. Links may lead out of the tree or into a cycle. */
    if ((fd->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        && (fd->dwReserved0 == IO_REPARSE_TAG_SYMLINK
            || fd->dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT)) {
        return HD_RDIR_TYPE_SYMLINK;
    }
    if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return HD_RDIR_TYPE_BEGIN_DIRECTORY;
    }
    return HD_RDIR_TYPE_FILE;
}


hd_rdir_t hd_rdir_open(const char *root_path)
{
    if (root_path == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (root_path[0] == '\0') {
        errno = ENOENT;
        return NULL;
    }

    hd_rdir_t it = calloc(1, sizeof(*it));
    if (it == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    it->codepage = win32_code_page();
    if (set_root_path(it, root_path) != 0) {
        goto fail;
    }

    wchar_t *root = win32_make_long_path(it->codepage, root_path);
    if (root == NULL) {
        goto fail;
    }

    /* Without trailing separators, so that '\' + name can be appended. */
    size_t len = wcslen(root);
    while (len > 0 && (root[len - 1] == L'\\' || root[len - 1] == L'/')) {
        len--;
    }
    if (reserve_wpath(it, len + 1) != 0) {
        free(root);
        goto fail;
    }
    memcpy(it->wpath, root, len * sizeof(wchar_t));
    it->wpath[len] = L'\0';
    it->wpath_len = len;
    it->wroot_len = len;
    free(root);

    /* The root itself may be a link; only links inside the tree are not followed. */
    if (push_dir(it, it->wpath_len, it->root_len, it->root_len) != 0) {
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
    free(it->wpath);
    free(it);
}

void hd_rdir_rewind(hd_rdir_t it)
{
    /*
     * A search handle can't be rewound, and searching the root again can
     * fail. So this only closes everything; the next hd_rdir_next()
     * searches the root again and reports any error.
     */
    while (it->depth > 0) {
        pop_frame(it);
    }
    it->path[it->root_len] = '\0';
    it->error = 0;
    it->restart = 1;
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
    if (it->restart) {
        /* Search the root again, as hd_rdir_open() did. */
        it->restart = 0;
        it->wpath_len = it->wroot_len;
        it->wpath[it->wroot_len] = L'\0';
        if (push_dir(it, it->wroot_len, it->root_len, it->root_len) != 0) {
            return fail(it, errno);
        }
    }

    while (it->depth > 0) {
        hd_rdir_frame *top = &it->stack[it->depth - 1];

        if (!top->has_data) {
            DWORD err = ERROR_NO_MORE_FILES;
            if (top->find != INVALID_HANDLE_VALUE && FindNextFileW(top->find, &top->data)) {
                err = ERROR_SUCCESS;
            } else if (top->find != INVALID_HANDLE_VALUE) {
                err = GetLastError();
            }

            if (err == ERROR_NO_MORE_FILES) {
                /* The directory is finished. The stack index is its entries' depth - 1. */
                size_t level = it->depth - 1;
                size_t path_len = top->path_len;
                size_t name_off = top->name_off;
                pop_frame(it);
                if (level == 0) {
                    break;  /* the root: the iteration is complete */
                }
                it->path[path_len] = '\0';
                return set_entry(it, name_off, HD_RDIR_TYPE_END_DIRECTORY, (int)level);
            }
            if (err != ERROR_SUCCESS) {
                return fail(it, win32_errno_from_error(err));
            }
        }
        top->has_data = 0;

        const wchar_t *name = top->data.cFileName;
        if (name[0] == L'.' && (name[1] == L'\0' || (name[1] == L'.' && name[2] == L'\0'))) {
            continue;
        }

        if (set_entry_wpath(it, top->wpath_len, name) != 0) {
            return fail(it, errno);
        }

        /* cFileName holds at most MAX_PATH - 1 UTF-16 units, each at most 3 bytes. */
        char narrow_name[3 * MAX_PATH + 1];
        int n = win32_wide_to_narrow(it->codepage, name, (int)wcslen(name),
                                     narrow_name, (int)sizeof(narrow_name) - 1);
        if (n < 0) {
            return fail(it, errno);
        }
        narrow_name[n] = '\0';

        /* Replace the previous entry's name with this one. */
        it->path[top->path_len] = '\0';
        errno = 0;
        if (hd_path_append(it->path, sizeof(it->path), narrow_name) < 0) {
            return fail(it, (errno != 0) ? errno : ENAMETOOLONG);
        }
        size_t path_len = strlen(it->path);
        size_t name_off = path_len - (size_t)n;
        int level = (int)it->depth;

        hd_rdir_type_t type = entry_type(&top->data);
        if (type == HD_RDIR_TYPE_BEGIN_DIRECTORY) {
            /* Entered right away, so BEGIN is only reported for directories that can be read. */
            if (push_dir(it, it->wpath_len, path_len, name_off) != 0) {
                return fail(it, errno);
            }
        }
        return set_entry(it, name_off, type, level);
    }

    errno = 0;
    return NULL;
}

#endif  /* _WIN32 */
