//
//  Inode.h
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Inode_h
#define Inode_h

#include <klib/klib.h>
#include "Lock.h"

CLASS_FORWARD(Filesystem);

typedef struct _file_info_t FileInfo;
typedef struct _mutable_file_info_t MutableFileInfo;

enum {
    kFilePermission_Read = 0x04,
    kFilePermission_Write = 0x02,
    kFilePermission_Execute = 0x01
};

enum {
    kFilePermissionScope_BitWidth = 3,

    kFilePermissionScope_User = 2*kFilePermissionScope_BitWidth,
    kFilePermissionScope_Group = kFilePermissionScope_BitWidth,
    kFilePermissionScope_Other = 0,

    kFilePermissionScope_Mask = 0x07,
};

#define FilePermissions_Make(__user, __group, __other) \
  (((__user) & kFilePermissionScope_Mask) << kFilePermissionScope_User) \
| (((__group) & kFilePermissionScope_Mask) << kFilePermissionScope_Group) \
| (((__other) & kFilePermissionScope_Mask) << kFilePermissionScope_Other)

#define FilePermissions_MakeFromOctal(__3_x_3_octal) \
    (__3_x_3_octal)
    
#define FilePermissions_Get(__permissions, __scope) \
(((__permissions) >> (__scope)) & kFilePermissionScope_Mask)

#define FilePermissions_Set(__permissions, __scope, __bits) \
((__permissions) & ~(kFilePermissionsScope_Mask << (__scope))) | (((__bits) & kFilePermissionsScope_Mask) << (__scope))


// The Inode type.
typedef enum _InodeType {
    kInode_RegularFile = 0,     // A regular file that stores data
    kInode_Directory,           // A directory which stores information about child nodes
} InodeType;


// Inode flags
enum {
    kInodeFlag_IsMountpoint = 1,    // owned and protected by the FilesystemManager
};


// An Inode represents the meta information of a file or directory. This is an
// abstract class that must be subclassed and fully implemented by a file system.
// See the description of the Filesystem class to learn about how locking for
// Inodes works.
OPEN_CLASS_WITH_REF(Inode, Object,
    Lock            lock;
    FileType        type;
    UInt8           flags;
    FilePermissions permissions;
    User            user;
    InodeId         inid;   // Filesystem specific ID of the inode
    FilesystemId    fsid;   // Globally unique ID of the filesystem that owns this node
    FileOffset      size;   // File size
);
typedef struct _InodeMethodTable {
    ObjectMethodTable   super;
} InodeMethodTable;


//
// Only filesystem implementations should call the following functions. They do
// not have any locking builtin. It is the job of the filesystem implementation
// to add and use locking.
//

// Creates an instance of the abstract Inode class. Should only ever be called
// by the implement of a creation function for a concrete Inode subclass.
extern ErrorCode Inode_AbstractCreate(ClassRef _Nonnull pClass, FileType type, InodeId id, FilesystemId fsid, FilePermissions permissions, User user, FileOffset size, InodeRef _Nullable * _Nonnull pOutNode);

#define Inode_Lock(__self) \
    Lock_Lock(&((InodeRef)__self)->lock)

#define Inode_Unlock(__self) \
    Lock_Unlock(&((InodeRef)__self)->lock)


//
// The following methods may only be called while holding the inode lock.
//

// Returns the permissions of the node.
#define Inode_GetFilePermissions(__self) \
    ((InodeRef)__self)->permissions

#define Inode_SetFilePermissions(__self, __perms) \
    ((InodeRef)__self)->permissions = __perms

// Returns the user of the node.
#define Inode_GetUser(__self) \
    ((InodeRef)__self)->user

#define Inode_SetUser(__self, __user) \
    ((InodeRef)__self)->user = __user

// Returns the User ID of the node.
#define Inode_GetUserId(__self) \
    ((InodeRef)__self)->user.uid

// Returns the group ID of the node.
#define Inode_GetGroupId(__self) \
    ((InodeRef)__self)->user.gid

// Returns EOK if the given user has at least the permissions 'permission' to
// access and/or manipulate the node; a suitable error code otherwise. The
// 'permission' parameter represents a set of the permissions of a single
// permission scope.
extern ErrorCode Inode_CheckAccess(InodeRef _Nonnull self, User user, FilePermissions permission);

#define Inode_GetFileSize(__self) \
    ((InodeRef)__self)->size

#define Inode_SetFileSize(__self, __size) \
    ((InodeRef)__self)->size = __size

#define Inode_IncrementFileSize(__self, __delta) \
    ((InodeRef)__self)->size += (__delta)

#define Inode_DecrementFileSize(__self, __delta) \
    ((InodeRef)__self)->size -= (__delta)

// Returns a file info record from the node data.
extern void Inode_GetFileInfo(InodeRef _Nonnull self, FileInfo* _Nonnull pOutInfo);

// Modifies the node's file info if the operation is permissible based on the
// given user and inode permissions status.
extern ErrorCode Inode_SetFileInfo(InodeRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo);
 

//
// Only the FilesystemManager should call the following functions.
//

#define Inode_IsMountpoint(__self) \
    (((InodeRef)__self)->flags & kInodeFlag_IsMountpoint) != 0

#define Inode_SetMountpoint(__self, __flag) \
    if(__flag) {((InodeRef)__self)->flags |= kInodeFlag_IsMountpoint;} else {((InodeRef)__self)->flags &= ~kInodeFlag_IsMountpoint;}

    
//
// The following functions may be used by any code and may be called without
// taking the inode lock first.
//

// Returns the type of the node.
#define Inode_GetType(__self) \
    ((InodeRef)__self)->type

// Returns true if the node is a directory; false otherwise.
#define Inode_IsDirectory(__self) \
    (Inode_GetType(__self) == kInode_Directory)

// Returns the filesystem specific ID of the node.
#define Inode_GetId(__self) \
    ((InodeRef)__self)->inid

// Returns the ID of the filesystem to which this node belongs.
#define Inode_GetFilesystemId(__self) \
    ((InodeRef)__self)->fsid

// Returns a strong reference to the filesystem that owns the given note. Returns
// NULL if the filesystem isn't mounted.
extern FilesystemRef Inode_CopyFilesystem(InodeRef _Nonnull self);

// Returns true if the receiver and 'pOther' are the same node; false otherwise
extern Bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther);

#endif /* Inode_h */
