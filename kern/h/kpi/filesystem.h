//
//  kpi/filesystem.h
//  kpi
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_FILESYSTEM_H
#define _KERN_FILESYSTEM_H 1

#include <kpi/types.h>

// Types of mountable objects
#define FS_MOUNT_CATALOG    ".catalog"
#define FS_MOUNT_SEFS       "sefs"

// Mountable catalogs
#define FS_CATALOG_DRIVERS  "dev"

// Unmount flags
#define FS_UNMOUNT_FORCE    0x0001   /* Force the unmount even if there are still files open */


// Filesystem specific information
#define FS_INFO_BASIC   1
#define FS_INFO_DISK    2

typedef void* fs_info_ref;

typedef struct fs_basic_info {
    blkcnt_t        capacity;       // Filesystem capacity in terms of filesystem blocks (if a regular fs) or catalog entries (if a catalog)
    blkcnt_t        count;          // Blocks or entries currently in use/allocated
    size_t          blockSize;      // Size of a block in bytes
    fsid_t          fsid;           // Filesystem ID
    unsigned int    flags;          // Filesystem flags
    char            type[12];       // Filesystem type (max 11 characters C string)
} fs_basic_info_t;

// Filesystem flags
#define FS_FLAG_CATALOG     0x0001  /* Filesystem is a kernel managed catalog */
#define FS_FLAG_REMOVABLE   0x0002  /* Filesystem lives on a removable/ejectable media */
#define FS_FLAG_READ_ONLY   0x0004  /* Filesystem was mounted read-only */


// Filesystem properties
#define FS_PROP_LABEL       1
#define FS_PROP_DISKPATH    2

#endif /* _KERN_FILESYSTEM_H */
