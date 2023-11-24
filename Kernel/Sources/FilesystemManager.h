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


// Creates the filesystem manager. The provided filesystem becomes the root of
// the global filesystem. Also known as "/".
extern ErrorCode FilesystemManager_Create(FilesystemRef _Nonnull pRootFileSys, FilesystemManagerRef _Nullable * _Nonnull pOutManager);

// Returns a strong reference to the root of the global filesystem.
extern FilesystemRef _Nonnull FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull pManager);

// Returns the node that represents the root of the global filesystem.
extern InodeRef _Nonnull FilesystemManager_CopyRootNode(FilesystemManagerRef _Nonnull pManager);

// Returns the filesystem for the given filesystem ID. NULL is returned if no
// file system for the given ID is registered/mounted anywhere in the global
// namespace.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid);

// Returns the node that mounts the filesystem with the given filesystem ID. Also
// returns the filesystem that owns the mounting node. ENOENT and NULLs are
// returned if the filesystem was never mounted or is no longer mounted. EOK and
// NULLs are returned if 'fsid' is the root filesystem (it has no parent file
// system).
extern ErrorCode FilesystemManager_CopyNodeAndFilesystemMountingFilesystemId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid, InodeRef _Nullable * _Nonnull pOutNode, FilesystemRef _Nullable * _Nonnull pOutFilesystem);

// Mounts the given filesystem at the given node. The node must be a directory
// node. The same filesystem instance may be mounted at multiple different
// directories.
extern ErrorCode FilesystemManager_Mount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, const Byte* _Nonnull pParams, ByteCount paramsSize, InodeRef _Nonnull pDirNode);

// Unmounts the given filesystem from the given directory.
extern ErrorCode FilesystemManager_Unmount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull pDirNode);

#endif /* FilesystemManager_h */
