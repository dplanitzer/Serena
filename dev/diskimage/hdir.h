//
//  hdir.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef HDIR_H
#define HDIR_H

/*
 * Recursive, depth-first iterator over a directory tree.
 *
 * Every file system object below the root is reported once. A directory is
 * reported twice: with HDIR_TYPE_BEGIN_DIRECTORY when the iterator
 * enters it, before its contents, and with HDIR_TYPE_END_DIRECTORY when
 * it leaves it, after its contents. The root itself and the "." / ".."
 * entries are never reported. Entries within a directory are reported in
 * the order the file system returns them.
 *
 * Each entry has a full path that can be passed to file system calls
 * ('root_path' + separator + relative path, e.g. "src/a/b/c.txt") and,
 * pointing into it, the path relative to the root ("a/b/c.txt") and the
 * name ("c.txt"). Paths are built with host_path_append(): the separator is
 * '/' on POSIX and '\' on Windows, where none is inserted after a bare drive
 * ("C:" + "a" is "C:a", relative to the drive's current directory).
 *
 * Links are reported but never followed, so link cycles cannot cause
 * infinite recursion:
 *   POSIX:   symbolic links.
 *   Windows: symbolic links and junctions / mount points. Other reparse
 *            points (e.g. OneDrive placeholders) are descended into.
 *
 * Windows: 'root_path' and the returned paths use the same narrow encoding
 * as the C runtime's fopen(); see host_code_page(). The encoding is chosen
 * once in hdir_open().
 */

#include <limits.h>

/*
 * Size in bytes of the iterator's path buffer, including the terminating
 * NUL. An entry's full path must fit, otherwise the iteration ends with
 * ENAMETOOLONG. Define it when compiling the rdir implementation to use
 * a different size.
 */
#ifndef HDIR_PATH_MAX
#if defined(_WIN32)
/* Paths of up to 4096 characters, each at most 3 bytes in any narrow encoding, plus the NUL. */
#define HDIR_PATH_MAX (3 * 4096 + 1)
#elif defined(PATH_MAX)
#define HDIR_PATH_MAX PATH_MAX
#else
#define HDIR_PATH_MAX 4096      /* e.g. GNU Hurd, which has no fixed limit */
#endif
#endif

/* An iterator; hdir_open() returns one, hdir_close() frees it. */
typedef struct hdir *hdir_t;

/* Kind of entry. */
typedef enum hdir_type {
    HDIR_TYPE_FILE,             /* regular file */
    HDIR_TYPE_BEGIN_DIRECTORY,  /* entering a directory; its contents follow */
    HDIR_TYPE_END_DIRECTORY,    /* leaving a directory; all its contents were reported */
    HDIR_TYPE_SYMLINK,          /* symbolic link; Windows: also junction / mount point */
    HDIR_TYPE_OTHER             /* POSIX only: FIFO, socket, device, or the type
                                   could not be determined */
} hdir_type_t;

typedef struct hdir_entry {
    const char      *full_path; /* e.g. "src/a/b/c.txt" */
    const char      *rel_path;  /* points into 'full_path': relative to the root, e.g. "a/b/c.txt" */
    const char      *name;      /* points into 'full_path': the last component, e.g. "c.txt" */
    hdir_type_t      type;
    int              depth;     /* 1 for entries directly inside the root, 2 for
                                   entries inside those directories, ... Both
                                   entries of a directory have the same depth. */
} hdir_entry_t;

/*
 * Creates an iterator for the tree rooted at 'root_path'.
 * Returns NULL and sets errno if the root can not be opened, is too long
 * for HDIR_PATH_MAX (ENAMETOOLONG) or memory can not be allocated.
 * 
 * hdir_t hdir_open(const char *root_path);
 */
#ifdef _WIN32
#define hdir_open(__root_path) \
win32_hdir_open(__root_path)
#else
#define hdir_open(__root_path) \
posix_hdir_open(__root_path)
#endif

/*
 * Frees all resources owned by the iterator. Passing NULL is allowed.
 *
 * void hdir_close(hdir_t it);
 */
#ifdef _WIN32
#define hdir_close(__it) \
win32_hdir_close(__it)
#else
#define hdir_close(__it) \
posix_hdir_close(__it)
#endif

/*
 * Resets the iterator to the start, wherever it currently is and also after
 * the iteration ended or failed: the next hdir_next() returns the first
 * entry of the root directory again, as after hdir_open(). Entries
 * returned earlier become invalid.
 *
 * This can't fail. On POSIX the root directory stays open until
 * hdir_close() and is rewound. On Windows it is searched again by the
 * next hdir_next(), which reports any error (e.g. if the root was
 * deleted in the meantime).
 * 
 * void hdir_rewind(hdir_t it);
 */
#ifdef _WIN32
#define hdir_rewind(__it) \
win32_hdir_rewind(__it)
#else
#define hdir_rewind(__it) \
posix_hdir_rewind(__it)
#endif

/*
 * Advances to the next entry and returns it. The entry and the strings it
 * points to belong to the iterator and stay valid until the next call to
 * hdir_next() or hdir_close().
 *
 * HDIR_TYPE_BEGIN_DIRECTORY is only returned once the directory was
 * opened successfully, so every BEGIN entry is followed by a matching END
 * entry unless an error ends the iteration first.
 *
 * Returns NULL when there are no more entries (errno is then set to 0) or
 * when an error occurred (errno is nonzero). An error ends the iteration:
 * all further calls return NULL with the same errno, until
 * hdir_rewind() is called.
 *   ENAMETOOLONG  a full path plus its NUL doesn't fit in HDIR_PATH_MAX
 *                 bytes
 *   EILSEQ        (Windows) a name can not be represented in the narrow
 *                 encoding
 *   other         a directory could not be opened or read (e.g. EACCES)
 * 
 * const hdir_entry_t *hdir_next(hdir_t it);
 */
#ifdef _WIN32
#define hdir_next(__it) \
win32_hdir_next(__it)
#else
#define hdir_next(__it) \
posix_hdir_next(__it)
#endif


//
// Private
//

#ifdef _WIN32
extern hdir_t win32_hdir_open(const char *root_path);
extern void win32_hdir_close(hdir_t it);
extern void win32_hdir_rewind(hdir_t it);
extern const hdir_entry_t *win32_hdir_next(hdir_t it);
#else
extern hdir_t posix_hdir_open(const char *root_path);
extern void posix_hdir_close(hdir_t it);
extern void posix_hdir_rewind(hdir_t it);
extern const hdir_entry_t *posix_hdir_next(hdir_t it);
#endif

#endif /* HDIR_H */
