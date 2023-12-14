//
//  FilesystemManager.h
//  Apollo
//
//  Created by Dietmar Planitzer on 11/09/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef FilesystemManager_h
#define FilesystemManager_h

#include <klib/klib.h>
#include "Filesystem.h"

struct _FilesystemManager;
typedef struct _FilesystemManager* FilesystemManagerRef;


extern FilesystemManagerRef _Nonnull  gFilesystemManager;


// Creates the filesystem manager. The provided filesystem is automatically
// mounted as the root filesystem.
extern ErrorCode FilesystemManager_Create(FilesystemRef _Nonnull pRootFileSys, FilesystemManagerRef _Nullable * _Nonnull pOutManager);

// Returns a strong reference to the root of the global filesystem.
extern FilesystemRef _Nonnull FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull pManager);

// Returns the node that represents the root of the global filesystem. Note that
// this function may fail if the root node of the root filesystem can not be read
// from disk.
extern ErrorCode FilesystemManager_CopyRootNode(FilesystemManagerRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutNode);

// Returns the filesystem for the given filesystem ID. NULL is returned if no
// file system for the given ID is registered/mounted anywhere in the global
// namespace.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid);

// Returns the mountpoint information of the node and filesystem that mounts the
// given filesystem. ENOENT and NULLs are returned if the filesystem was never
// mounted or is no longer mounted. EOK and NULLs are returned if 'pFileSys' is
// the root filesystem (it has no parent file system).
extern ErrorCode FilesystemManager_CopyMountpointOfFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nullable * _Nonnull pOutMountingNode, FilesystemRef _Nullable * _Nonnull pOutMountingFilesystem);

// Checks whether the given node is a mount point and returns the filesystem
// mounted at that node, if it is. Otherwise returns NULL.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemMountedAtNode(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull pNode);

// Mounts the given filesystem at the given node. The node must be a directory
// node. A filesystem instance may be mounted at at most one directory.
extern ErrorCode FilesystemManager_Mount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, const Byte* _Nonnull pParams, ByteCount paramsSize, InodeRef _Nonnull pDirNode);

// Unmounts the given filesystem from the given directory.
extern ErrorCode FilesystemManager_Unmount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull pDirNode);

// This function should be called from a filesystem onUnmount() implementation
// to verify that the unmount can be completed safely. Eg no more open files
// exist that reference the filesystem. Note that the filesystem must do this
// call as part of its atomic unmount sequence. Eg the filesystem must lock its
// state, ensure that any operations that might have been ongoing have completed,
// then call this function before proceeding with the unmount.
extern Bool FilesystemManager_CanSafelyUnmountFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys);
 
#endif /* FilesystemManager_h */
