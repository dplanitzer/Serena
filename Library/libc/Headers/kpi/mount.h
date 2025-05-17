//
//  kpi/mount.h
//  libc
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_MOUNT_H
#define _KPI_MOUNT_H 1

// Types of mountable objects
#define kMount_Catalog  ".catalog"
#define kMount_SeFS     "sefs"


// Mountable catalogs
#define kCatalogName_Drivers        "dev"
#define kCatalogName_Filesystems    "fs"
#define kCatalogName_Processes      "proc"


enum {
    kUnmount_Forced = 0x0001,   // Force the unmount even if there are still files open
};
typedef unsigned int UnmountOptions;

#endif /* _KPI_MOUNT_H */
