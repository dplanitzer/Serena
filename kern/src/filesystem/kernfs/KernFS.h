//
//  KernFS.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef KernFS_h
#define KernFS_h

#include <filesystem/Filesystem.h>
#include <kpi/ioctl.h>

#define KERNFS_NAME_MAX 7

final_class(KernFS, Filesystem);


// Creates an instance of KernFS.
extern errno_t KernFS_Create(const char* _Nonnull name, KernFSRef _Nullable * _Nonnull pOutSelf);

// Creates a new driver node in the file system.
extern errno_t KernFS_CreateDriverNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, DriverRef _Nonnull drv, uid_t uid, gid_t gid, fs_perms_t permissions, InodeRef _Nullable * _Nonnull pOutNode);

// Returns a snapshot of strong references to all drivers that match the provided
// categories. The caller is responsible for releasing all references and calling
// kfree() on the returned pointer when done. The array of driver references is
// terminated by a NULL entry.
extern errno_t KernFS_CopyMatchingDrivers(KernFSRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nullable * _Nonnull pOutDrivers);

// Same as above but limited to the best matching driver.
extern DriverRef _Nullable KernFS_CopyBestMatchingDriver(KernFSRef _Nonnull self, const iocat_t* _Nonnull cats);

#endif /* KernFS_h */
