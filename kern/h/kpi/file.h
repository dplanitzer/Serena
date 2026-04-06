//
//  kpi/file.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_FILE_H
#define _KPI_FILE_H 1

#include <kpi/_file.h>
#include <kpi/syslimits.h>
#include <kpi/_time.h>
#include <kpi/types.h>

typedef struct fs_attr {
    struct timespec st_atim;    // Last data access time
    struct timespec st_mtim;    // Last data modification time
    struct timespec st_ctim;    // Last file status change time
    off_t           st_size;
    uid_t           st_uid;
    gid_t           st_gid;
    mode_t          st_mode;
    nlink_t         st_nlink;
    fsid_t          st_fsid;
    ino_t           st_ino;
    blksize_t       st_blksize;
    blkcnt_t        st_blocks;
    dev_t           st_dev;     // Always 0
    dev_t           st_rdev;    // Always 0
} fs_attr_t;


// Inode type.
#define S_IFREG     0x00000000  /* A regular file that stores data */
#define S_IFDIR     0x01000000  /* A directory which stores information about child nodes */
#define S_IFDEV     0x02000000  /* A driver which manages a piece of hardware */
#define S_IFPROC    0x03000000  /* A process */
#define S_IFLNK     0x04000000
#define S_IFIFO     0x05000000

// Convenience macros to check for inode types
#define S_ISREG(__mode)     (((__mode) & S_IFMT) == S_IFREG)
#define S_ISDIR(__mode)     (((__mode) & S_IFMT) == S_IFDIR)
#define S_ISDEV(__mode)     (((__mode) & S_IFMT) == S_IFDEV)
#define S_ISPROC(__mode)    (((__mode) & S_IFMT) == S_IFPROC)
#define S_ISLNK(__mode)     (((__mode) & S_IFMT) == S_IFLNK)
#define S_ISFIFO(__mode)    (((__mode) & S_IFMT) == S_IFIFO)

// Returns the file permission bits from a fs_attr_t
#define S_FPERM(__mode) ((__mode) & S_IFMP)

// Returns the file type bits from a fs_attr_t
#define S_FTYPE(__mode) ((__mode) & S_IFMT)


// Tells fs_settimes() to set the file timestamp to the current time. Assign to
// the tv_nsec field.
#define FS_TIME_NOW   -1

// Tells fs_settimes() to set the file timestamp to leave the file timestamp
// unchanged. Assign to the tv_nsec field.
#define FS_TIME_OMIT  -2


// Order of the fs_settimes() timestamps
#define FS_TIMFLD_ACC   0   /* Access time */
#define FS_TIMFLD_MOD   1   /* Modification time */


// __fs_unlink() system call mode
#define __FS_ULNK_ANY       0
#define __FS_ULNK_FIL_ONLY  1
#define __FS_ULNK_DIR_ONLY  2

#endif /* _KPI_FILE_H */
