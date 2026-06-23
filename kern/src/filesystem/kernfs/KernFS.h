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

typedef errno_t (*KfsCreateHandlerFunc)(void* _Nonnull ctx, fd_flags_t flags, HandlerRef _Nullable * _Nullable pOutHandler);

typedef struct KfsHandlerNodeArgs {
    ObjectRef _Nullable             resource;
    void* _Nullable                 context;
    KfsCreateHandlerFunc _Nullable  func;
    fs_perms_t                      perms;
    uid_t                           uid;
    gid_t                           gid;
} KfsHandlerNodeArgs;


final_class(KernFS, Filesystem);


// Creates an instance of KernFS.
extern errno_t KernFS_Create(const char* _Nonnull name, KernFSRef _Nullable * _Nonnull pOutSelf);


// Creates a new node that has no backing resource and will invoke the provided handler factory function when needed.
extern errno_t KernFS_CreateHandlerNode(KernFSRef _Nonnull self, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, const KfsHandlerNodeArgs* _Nonnull args, InodeRef _Nullable * _Nonnull pOutNode);


// Returns a snapshot of strong references to all drivers that match the provided
// categories. The caller is responsible for releasing all references and calling
// kfree() on the returned pointer when done. The array of driver references is
// terminated by a NULL entry.
extern errno_t KernFS_CopyMatchingDrivers(KernFSRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nullable * _Nonnull pOutDrivers);

// Same as above but limited to the best matching driver.
extern DriverRef _Nullable KernFS_CopyBestMatchingDriver(KernFSRef _Nonnull self, const iocat_t* _Nonnull cats);

#endif /* KernFS_h */
