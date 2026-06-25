//
//  devfs.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_DEVFS_H_
#define _KERN_DEVFS_H_

#include <kpi/fs_perms.h>
#include <kpi/ioctl.h>
#include <kpi/types.h>
#include <filemanager/ResolvedPath.h>


typedef errno_t (*devfs_handler_func_t)(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nullable pOutHandler);


typedef struct devfs_entry {
    const char* _Nonnull            name;
    ObjectRef _Nullable             resource;
    devfs_handler_func_t _Nonnull   func;
    uid_t                           uid;
    gid_t                           gid;
    fs_perms_t                      perms;
} devfs_entry_t;


// The non-persistent, globally unique handle of a devfs entry. This handle does
// not survive a system reboot. Id 0 represents a devfs entry that does not exist.
typedef uint32_t    devfs_hnd_t;



extern errno_t devfs_init(void);


extern FilesystemRef _Nonnull devfs_get_fs(void);

// Returns the inode that represents the given handle. A suitable error is
// returned if the handle has no inode associated with it, etc. You must call
// Inode_Relinquish() when you no longer need the inode. Returns EINVAL if 'hnd'
// is 0.
extern errno_t devfs_acquire_node_for_handle(devfs_hnd_t hnd, InodeRef _Nullable * _Nonnull pOutNode);


// Publishes a new special file entry in devfs. The entry may be associated
// with an already existing resource 'resource'. The function 'func' is called
// when a handler for the resource is needed. 'resource' may be NULL.
extern errno_t devfs_add(const devfs_entry_t* _Nonnull ce, devfs_hnd_t* _Nullable pOutHnd);

// Removes the entry associated with the given handle from devfs.
extern errno_t devfs_remove(devfs_hnd_t hnd);

#endif /* _KERN_DEVFS_H_ */
