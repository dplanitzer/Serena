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
#include <kpi/ioctl.h>

#define KERNFS_NAME_MAX 7

typedef errno_t (*KfsCreateHandlerFunc)(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nullable pOutHandler);

typedef struct KfsHandlerNodeArgs {
    ObjectRef _Nullable             resource;
    KfsCreateHandlerFunc _Nonnull   func;
    fs_perms_t                      perms;
    uid_t                           uid;
    gid_t                           gid;
} KfsHandlerNodeArgs;


final_class(KernFS, Filesystem);


// Creates an instance of KernFS.
extern errno_t KernFS_Create(const char* _Nonnull name, KernFSRef _Nullable * _Nonnull pOutSelf);


// Creates a new node that has no backing resource and will invoke the provided handler factory function when needed.
extern errno_t KernFS_CreateHandlerNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, const KfsHandlerNodeArgs* _Nonnull args, InodeRef _Nullable * _Nonnull pOutNode);

#endif /* KernFS_h */
