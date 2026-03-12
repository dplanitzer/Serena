//
//  sys/fs.h
//  libc
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FS_H
#define _SYS_FS_H 1

#include <_cmndef.h>
#include <kpi/fs.h>


// Mounts the object 'objectName' of type 'objectType' at the directory 'atDirPath'.
// 'params' are optional mount parameters that are passed to the filesystem to
// mount.
extern int mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params);

// Unmounts the filesystem mounted at the directory 'atDirPath'.
extern int unmount(const char* _Nonnull atDirPath, UnmountOptions options);

// Returns the path to the disk driver that underpins the filesystem with the
// given id.
extern int fs_getdisk(fsid_t fsid, char* _Nonnull buf, size_t bufSize);

// Synchronously writes all dirty disk blocks back to disk.
extern void sync(void);

#endif /* _SYS_FS_H */
