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

// Creates a new device node in the file system.
extern errno_t KernFS_CreateDeviceNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, ObjectRef _Nonnull drvOrHnd, intptr_t arg, uid_t uid, gid_t gid, mode_t permissions, InodeRef _Nullable * _Nonnull pOutNode);

// Creates a new filesystem node in the file system.
extern errno_t KernFS_CreateFilesystemNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, FilesystemRef _Nonnull fs, uid_t uid, gid_t gid, mode_t permissions, InodeRef _Nullable * _Nonnull pOutNode);

// Creates a new process node in the file system.
extern errno_t KernFS_CreateProcessNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, ProcessRef _Nonnull proc, uid_t uid, gid_t gid, mode_t permissions, InodeRef _Nullable * _Nonnull pOutNode);

#endif /* KernFS_h */
