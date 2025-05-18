//
//  kpi/stat.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_STAT_H
#define _KPI_STAT_H 1

#include <kpi/_stat.h>
#include <kpi/_time.h>
#include <kpi/syslimits.h>
#include <kpi/types.h>

#define PATH_MAX __PATH_MAX
#define NAME_MAX __PATH_COMPONENT_MAX


typedef char            FileType;


typedef struct finfo {
    struct timespec     accessTime;
    struct timespec     modificationTime;
    struct timespec     statusChangeTime;
    off_t               size;
    uid_t               uid;
    gid_t               gid;
    mode_t              permissions;
    FileType            type;
    char                reserved;
    nlink_t             linkCount;
    fsid_t              fsid;
    ino_t               inid;
} finfo_t;


// The Inode type.
#define S_IFREG     0   /* A regular file that stores data */
#define S_IFDIR     1   /* A directory which stores information about child nodes */
#define S_IFDEV     2   /* A driver which manages a piece of hardware */
#define S_IFFS      3   /* A mounted filesystem instance */
#define S_IFPROC    4   /* A process */
#define S_IFLNK     5
#define S_IFIFO     6


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
