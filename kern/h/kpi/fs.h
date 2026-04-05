//
//  kpi/fs.h
//  kpi
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_FS_H
#define _KERN_FS_H 1

#include <kpi/types.h>

// Types of mountable objects
#define kMount_Catalog  ".catalog"
#define kMount_SeFS     "sefs"


// Mountable catalogs
#define kCatalogName_Drivers    "dev"
#define kCatalogName_Processes  "proc"


enum {
    kUnmount_Forced = 0x0001,   // Force the unmount even if there are still files open
};
typedef unsigned int UnmountOptions;



// Filesystem properties
enum {
    kFSProperty_IsCatalog = 0x0001,     // Filesystem is a kernel managed catalog
    kFSProperty_IsRemovable = 0x0002,   // Filesystem lives on a removable/ejectable media
    kFSProperty_IsReadOnly = 0x0004,    // Filesystem was mounted read-only
};


// Filesystem specific information
#define FS_INFO_BASIC   1
#define FS_INFO_DISK    2

typedef void* fs_info_ref;

typedef struct fs_basic_info {
    blkcnt_t        capacity;       // Filesystem capacity in terms of filesystem blocks (if a regular fs) or catalog entries (if a catalog)
    blkcnt_t        count;          // Blocks or entries currently in use/allocated
    size_t          blockSize;      // Size of a block in bytes
    fsid_t          fsid;           // Filesystem ID
    unsigned int    properties;     // Filesystem properties
    char            type[12];       // Filesystem type (max 11 characters C string)
} fs_basic_info_t;

#endif /* _KERN_FS_H */
