//
//  FilesystemManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/09/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef FilesystemManager_h
#define FilesystemManager_h

#include <klib/klib.h>
#include "Filesystem.h"

struct FilesystemManager;
typedef struct FilesystemManager* FilesystemManagerRef;


extern FilesystemManagerRef _Nonnull  gFilesystemManager;


// Creates the filesystem manager. The provided filesystem is automatically
// mounted as the root filesystem on the disk partition 'pDriver'.
extern errno_t FilesystemManager_Create(FilesystemRef _Nonnull pRootFileSys, DiskDriverRef _Nonnull pDriver, FilesystemManagerRef _Nullable * _Nonnull pOutManager);

// Returns a strong reference to the root of the global filesystem.
extern FilesystemRef _Nonnull FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull pManager);

// Returns the filesystem for the given filesystem ID. NULL is returned if no
// file system for the given ID is registered/mounted anywhere in the global
// namespace.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid);

// Returns the mountpoint information of the node and filesystem that mounts the
// given filesystem. ENOENT and NULLs are returned if the filesystem was never
// mounted or is no longer mounted. EOK and NULLs are returned if 'pFileSys' is
// the root filesystem (it has no parent file system).
extern errno_t FilesystemManager_CopyMountpointOfFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nullable _Locked * _Nonnull pOutMountingNode, FilesystemRef _Nullable * _Nonnull pOutMountingFilesystem);

// Returns true if the given node is a mountpoint and false otherwise.
extern bool FilesystemManager_IsNodeMountpoint(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull _Locked pNode);

// Checks whether the given node is a mount point and returns the filesystem
// mounted at that node, if it is. Otherwise returns NULL.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemMountedAtNode(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull _Locked pNode);

// Mounts the given filesystem physically located at the given disk partition
// and attaches it at the given node. The node must be a directory node. A
// filesystem instance may be mounted at at most one directory.
extern errno_t FilesystemManager_Mount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize, InodeRef _Nonnull _Locked pDirNode);

// Unmounts the given filesystem from the given directory.
extern errno_t FilesystemManager_Unmount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull _Locked pDirNode);
 
#endif /* FilesystemManager_h */
