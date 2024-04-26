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


// Creates the filesystem manager.
extern errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutManager);

// Returns a strong reference to the root of the global filesystem.
extern FilesystemRef _Nullable FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull pManager);

// Returns the filesystem for the given filesystem ID. NULL is returned if no
// file system for the given ID is registered/mounted anywhere in the global
// namespace.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid);

// Acquires the inode that is mounting the given filesystem instance. A suitable
// error and NULL is returned if the given filesystem is not mounted (anymore)
// or some other problem is detected.
// EOK and NULLs are returned if 'pFileSys' is the root filesystem (it has no
// parent file system).
extern errno_t FilesystemManager_AcquireNodeMountingFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nullable _Locked * _Nonnull pOutMountingNode);

// Returns true if the given node is a mountpoint and false otherwise.
extern bool FilesystemManager_IsNodeMountpoint(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull _Locked pNode);

// Checks whether the given node is a mount point and returns the filesystem
// mounted at that node, if it is. Otherwise returns NULL.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemMountedAtNode(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull _Locked pNode);

// Mounts the given filesystem physically located at the given disk partition
// and attaches it at the given node. The node must be a directory node. A
// filesystem instance may be mounted at at most one directory. If the node is
// NULL then the given filesystem is mounted as the root filesystem.
extern errno_t FilesystemManager_Mount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, DiskDriverRef _Nonnull pDriver, const void* _Nullable pParams, ssize_t paramsSize, InodeRef _Nullable _Locked pDirNode);

// Unmounts the given filesystem from the directory it is currently mounted on.
// Remember that one filesystem instance can be mounted at most once at any given
// time.
// A filesystem is only unmountable under normal circumstances if there are no
// more acquired inodes outstanding. Unmounting will fail with an EBUSY error if
// there is at least one acquired inode outstanding. However you may pass true
// for 'force' which forces the unmount. A forced unmount means that the
// filesystem will be immediately removed from the file hierarchy. However the
// unmounting and deallocation of the filesystem instance will be deferred until
// after the last outstanding inode has been relinquished.
extern errno_t FilesystemManager_Unmount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, bool force);
 
#endif /* FilesystemManager_h */
