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

// Starts the given filesystem instance. Passes 'params' as the start parameters
// to this filesystem.
extern errno_t FilesystemManager_StartFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, const char* _Nonnull params);

// Stops the filesystem 'fs'. If 'forced' is false and the filesystem is still
// in use (attached, inodes outstanding, open channels outstanding) then the
// filesystem will not be stopped and a suitable error is returned. If 'forced'
// is true and the filesystem is still in use then the filesystem will be force
// stopped and scheduled for future destruction. The filesystem will be
// destructed once all outstanding inodes and channels have been closed.
extern errno_t FilesystemManager_StopFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, bool forced);

// Syncs all filesystems and modified blocks to disk. Blocks until the sync is
// complete.
extern void FilesystemManager_Sync(FilesystemManagerRef _Nonnull self);

#endif /* FilesystemManager_h */
