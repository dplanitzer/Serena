//
//  dd_rdir.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef DD_RDIR_H
#define DD_RDIR_H

#include <limits.h>
#include <dd/dd_types.h>

/*
 * Recursive, depth-first iterator over a directory tree.
 *
 * Every file system object below the root is reported once. A directory is
 * reported twice: with RDIR_TYPE_BEGIN_DIRECTORY when the iterator
 * enters it, before its contents, and with RDIR_TYPE_END_DIRECTORY when
 * it leaves it, after its contents. The root itself and the "." / ".."
 * entries are never reported. Entries within a directory are reported in
 * the order the file system returns them.
 *
 * Each entry has a full path that can be passed to file system calls
 * ('root_path' + separator + relative path, e.g. "src/a/b/c.txt") and,
 * pointing into it, the path relative to the root ("a/b/c.txt") and the
 * name ("c.txt").
 *
 * Links are reported but never followed, so link cycles cannot cause
 * infinite recursion:
 *   POSIX:   symbolic links.
 */


/* An iterator; rdir_open() returns one, rdir_close() frees it. */
typedef struct dd_rdir *dd_rdir_t;

/* Kind of entry. */
typedef enum dd_rdir_type {
    DD_RDIR_TYPE_FILE,              /* regular file */
    DD_RDIR_TYPE_BEGIN_DIRECTORY,   /* entering a directory; its contents follow */
    DD_RDIR_TYPE_END_DIRECTORY,     /* leaving a directory; all its contents were reported */
    DD_RDIR_TYPE_SYMLINK,           /* symbolic link; Windows: also junction / mount point */
    DD_RDIR_TYPE_OTHER              /* FIFO, socket, device, or the type could not be determined */
} dd_rdir_type_t;

typedef struct dd_rdir_entry {
    const char      *full_path; /* e.g. "src/a/b/c.txt" */
    const char      *rel_path;  /* points into 'full_path': relative to the root, e.g. "a/b/c.txt" */
    const char      *name;      /* points into 'full_path': the last component, e.g. "c.txt" */
    dd_rdir_type_t   type;
    int              depth;     /* 1 for entries directly inside the root, 2 for
                                   entries inside those directories, ... Both
                                   entries of a directory have the same depth. */
} dd_rdir_entry_t;

/*
 * Creates an iterator for the tree rooted at 'root_path'.
 * Returns NULL and sets errno if the root can not be opened, is too long
 * for DD_PATH_MAX (ENAMETOOLONG) or memory can not be allocated.
 */
dd_rdir_t dd_rdir_open(dd_fs_t fs, const char *root_path);

/*
 * Frees all resources owned by the iterator. Passing NULL is allowed.
 */
void dd_rdir_close(dd_rdir_t it);

/*
 * Advances to the next entry and returns it. The entry and the strings it
 * points to belong to the iterator and stay valid until the next call to
 * rdir_next() or rdir_close().
 *
 * RDIR_TYPE_BEGIN_DIRECTORY is only returned once the directory was
 * opened successfully, so every BEGIN entry is followed by a matching END
 * entry unless an error ends the iteration first.
 *
 * Returns NULL when there are no more entries (errno is then set to 0) or
 * when an error occurred (errno is nonzero). An error ends the iteration:
 * all further calls return NULL with the same errno, until
 * rdir_rewind() is called.
 *   ENAMETOOLONG  a full path plus its NUL doesn't fit in DD_PATH_MAX
 *                 bytes
 *   other         a directory could not be opened or read (e.g. EACCES)
 */
const dd_rdir_entry_t *dd_rdir_next(dd_rdir_t it);

#endif /* DD_RDIR_H */
