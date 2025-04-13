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


final_class(KernFS, Filesystem);


// Creates an instance of KernFS.
extern errno_t KernFS_Create(KernFSRef _Nullable * _Nonnull pOutSelf);

// Returns a strong reference to the driver backing the given driver node.
// Returns NULL if the given node is not a driver node.
extern DriverRef _Nullable KernFS_CopyDriverForNode(KernFSRef _Nonnull self, InodeRef _Nonnull pNode);

// Creates a new device node in the file system.
extern errno_t KernFS_CreateDevice(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, DriverRef _Nonnull pDriverInstance, intptr_t arg, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);

// Creates a new filesystem node in the file system.
extern errno_t KernFS_CreateFilesystem(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, FilesystemRef _Nonnull fsInstance, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);

#endif /* KernFS_h */
