//
//  DevFS.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DevFS_h
#define DevFS_h

#include <filesystem/Filesystem.h>


final_class(DevFS, Filesystem);


// Creates an instance of DevFS.
extern errno_t DevFS_Create(DevFSRef _Nullable * _Nonnull pOutSelf);

// Returns a strong reference to the driver backing the given driver node.
// Returns NULL if the given node is not a driver node.
extern DriverRef _Nullable DevFS_CopyDriverForNode(DevFSRef _Nonnull self, InodeRef _Nonnull pNode);

// Creates a new device node in the file system.
extern errno_t DevFS_CreateDevice(DevFSRef _Nonnull self, User user, FilePermissions permissions, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, DriverRef _Nonnull pDriverInstance, intptr_t arg, InodeRef _Nullable * _Nonnull pOutNode);

#endif /* DevFS_h */
