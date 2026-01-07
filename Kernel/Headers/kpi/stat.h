//
//  kpi/stat.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_STAT_H
#define _KPI_STAT_H 1

#include <_time.h>
#include <kpi/_stat.h>
#include <kpi/syslimits.h>
#include <kpi/types.h>

#define PATH_MAX __PATH_MAX
#define NAME_MAX __PATH_COMPONENT_MAX


struct stat {
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
};


// The Inode type.
#define S_IFREG     0x00000000  /* A regular file that stores data */
#define S_IFDIR     0x01000000  /* A directory which stores information about child nodes */
#define S_IFDEV     0x02000000  /* A driver which manages a piece of hardware */
#define S_IFFS      0x03000000  /* A mounted filesystem instance */
#define S_IFPROC    0x04000000  /* A process */
#define S_IFLNK     0x05000000
#define S_IFIFO     0x06000000

// Convenience macros to check for inode types
#define S_ISREG(__mode)     (((__mode) & S_IFMT) == S_IFREG)
#define S_ISDIR(__mode)     (((__mode) & S_IFMT) == S_IFDIR)
#define S_ISDEV(__mode)     (((__mode) & S_IFMT) == S_IFDEV)
#define S_ISFS(__mode)      (((__mode) & S_IFMT) == S_IFFS)
#define S_ISPROC(__mode)    (((__mode) & S_IFMT) == S_IFPROC)
#define S_ISLNK(__mode)     (((__mode) & S_IFMT) == S_IFLNK)
#define S_ISFIFO(__mode)    (((__mode) & S_IFMT) == S_IFIFO)

// Returns the file permission bits from a struct stat
#define S_FPERM(__mode) ((__mode) & S_IFMP)

// Returns the file type bits from a struct stat
#define S_FTYPE(__mode) ((__mode) & S_IFMT)


// Tells utimens() to set the file timestamp to the current time. Assign to the
// tv_nsec field.
#define UTIME_NOW   -1

// Tells utimens() to set the file timestamp to leave the file timestamp
// unchanged. Assign to the tv_nsec field.
#define UTIME_OMIT  -2


// Order of the utimesns() timestamp
#define UTIME_ACCESS        0
#define UTIME_MODIFICATION  1


// Tell umask() to just return the current umask without changing it
#define SEO_UMASK_NO_CHANGE -1

#endif /* _KPI_STAT_H */
