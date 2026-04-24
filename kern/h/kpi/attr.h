//
//  kpi/attr.h
//  kpi
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_ATTR_H
#define _KPI_ATTR_H 1

#include <kpi/_time.h>
#include <kpi/fs_perms.h>
#include <kpi/types.h>

typedef struct fs_attr {
    nanotime_t  acc_time;    // Last data access time
    nanotime_t  mod_time;    // Last data modification time
    nanotime_t  chg_time;    // Last file status change time
    off_t       size;
    uid_t       uid;
    gid_t       gid;
    fs_ftype_t  file_type;
    fs_perms_t  permissions;
    nlink_t     nlink;
    fsid_t      fsid;
    ino_t       ino;
    blksize_t   blk_size;
    blkcnt_t    blocks;
} fs_attr_t;


// File type.
#define FS_FTYPE_REG    0   /* A regular file that stores data */
#define FS_FTYPE_DIR    1   /* A directory which stores information about child nodes */
#define FS_FTYPE_DEV    2   /* A driver which manages a piece of hardware */
#define FS_FTYPE_LNK    3
#define FS_FTYPE_FIFO   4

#endif /* _KPI_ATTR_H */
