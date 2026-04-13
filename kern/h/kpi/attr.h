//
//  kpi/attr.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_ATTR_H
#define _KPI_ATTR_H 1

#include <kpi/_attr.h>
#include <kpi/_time.h>
#include <kpi/types.h>

typedef struct fs_attr {
    struct timespec acc_time;    // Last data access time
    struct timespec mod_time;    // Last data modification time
    struct timespec chg_time;    // Last file status change time
    off_t           size;
    uid_t           uid;
    gid_t           gid;
    mode_t          mode;
    nlink_t         nlink;
    fsid_t          fsid;
    ino_t           ino;
    blksize_t       blk_size;
    blkcnt_t        blocks;
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

#endif /* _KPI_ATTR_H */
