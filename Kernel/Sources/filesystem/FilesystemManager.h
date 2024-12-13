//
//  FilesystemManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FilesystemManager_h
#define FilesystemManager_h

#include <filesystem/FileHierarchy.h>
#include <filesystem/Filesystem.h>


extern FilesystemManagerRef _Nonnull gFilesystemManager;

extern errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutSelf);

// Auto-discovers the filesystem stored in the mass storage device 'pDriver',
// instantiates and starts it. Passes 'params' as the start parameters to this
// filesystem.
extern errno_t FilesystemManager_DiscoverAndStartFilesystem(FilesystemManagerRef _Nonnull self, DiskDriverRef _Nonnull pDriver, const void* _Nullable params, size_t paramsSize, FilesystemRef _Nullable * _Nonnull pOutFs);

#endif /* FilesystemManager_h */
