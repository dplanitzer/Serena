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


// Returns the filesystem that represents the /fs catalog.
extern FilesystemRef _Nonnull FilesystemManager_GetCatalog(FilesystemManagerRef _Nonnull self);


// Establishes a filesystem stored on the given disk. This means that we
// create a FS container for the filesystem, instantiate it and record information
// about it.
extern errno_t FilesystemManager_EstablishFilesystem(FilesystemManagerRef _Nonnull self, InodeRef _Locked _Nonnull driverNode, unsigned int mode, FilesystemRef _Nullable * _Nonnull pOutFs);

// Starts the given filesystem instance. Passes 'params' as the start parameters
// to this filesystem.
extern errno_t FilesystemManager_StartFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, const char* _Nonnull params);

// Stops the filesystem 'fs'. If 'forced' is false and the filesystem is still
// in use (attached, inodes outstanding, open channels outstanding) then the
// filesystem will not be stopped and a suitable error is returned. If 'forced'
// is true and the filesystem is still in use then the filesystem will be force
// stopped anyway. Note that you should disband the filesystem next.
extern errno_t FilesystemManager_StopFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs, bool forced);

// Disbands the given filesystem. This means that we tell the filesystem to
// disconnect from its underlying storage. We then try to destroy it. If this
// isn't possible because there are still inodes and/or filesystem channels
// active then we schedule the filesystem for future destruction.
extern void FilesystemManager_DisbandFilesystem(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull fs);

// Returns the inode of the disk driver that underpins the filesystem for 'fsid'.
extern errno_t FilesystemManager_AcquireDriverNodeForFsid(FilesystemManagerRef _Nonnull self, fsid_t fsid, InodeRef _Nullable * _Nonnull pOutNode);

// Syncs all filesystems and modified blocks to disk. Blocks until the sync is
// complete.
extern void FilesystemManager_Sync(FilesystemManagerRef _Nonnull self);

#endif /* FilesystemManager_h */
