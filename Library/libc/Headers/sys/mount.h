//
//  sys/mount.h
//  libc
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H 1

#include <System/_cmndef.h>
#include <sys/types.h>

__CPP_BEGIN

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


// Mounts the object 'objectName' of type 'objectType' at the directory 'atDirPath'.
// 'params' are optional mount parameters that are passed to the filesystem to
// mount.
extern int mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params);

// Unmounts the filesystem mounted at the directory 'atDirPath'.
extern int unmount(const char* _Nonnull atDirPath, UnmountOptions options);

// Returns the path to the disk driver that underpins the filesystem with the
// given id.
extern int fs_getdisk(fsid_t fsid, char* _Nonnull buf, size_t bufSize);

__CPP_END

#endif /* _SYS_MOUNT_H */
