//
//  FilesystemManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FilesystemManager_h
#define FilesystemManager_h

#include <filemanager/FileHierarchy.h>
#include <filesystem/Filesystem.h>


extern FilesystemManagerRef _Nonnull gFilesystemManager;

extern errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutSelf);
extern errno_t FilesystemManager_Start(FilesystemManagerRef _Nonnull self);


// Registers the given filesystem.
extern errno_t FilesystemManager_RegisterFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs);

// Deregister the given filesystem. This means that we tell the filesystem to
// disconnect from its underlying storage. We then try to destroy it. If this
// isn't possible because there are still inodes and/or filesystem channels
// active then we schedule the filesystem for future destruction.
extern void FilesystemManager_DeregisterFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs);

// Returns a strong reference to the filesystem with the given id; NULL if no
// such filesystem exists.
extern FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull self, fsid_t id);

// Returns the ids of all currently mounted filesystems.
extern errno_t FilesystemManager_GetFilesystemIds(FilesystemManagerRef _Nonnull self, fsid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore);

// Returns the number of registered filesystems.
extern size_t FilesystemManager_GetFilesystemCount(FilesystemManagerRef _Nonnull self);

// Syncs all filesystems and modified blocks to disk. Blocks until the sync is
// complete.
extern void FilesystemManager_Sync(FilesystemManagerRef _Nonnull self);

#endif /* FilesystemManager_h */
