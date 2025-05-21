//
//  sys/mount.h
//  libc
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_MOUNT_H
#define _SYS_MOUNT_H 1

#include <_cmndef.h>
#include <sys/types.h>
#include <kpi/mount.h>

__CPP_BEGIN

// Mounts the object 'objectName' of type 'objectType' at the directory 'atDirPath'.
// 'params' are optional mount parameters that are passed to the filesystem to
// mount.
extern int mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params);

// Unmounts the filesystem mounted at the directory 'atDirPath'.
extern int unmount(const char* _Nonnull atDirPath, UnmountOptions options);

__CPP_END

#endif /* _SYS_MOUNT_H */
