//
//  Filesystem.h
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Filesystem_h
#define Filesystem_h

#include "IOResource.h"

CLASS_FORWARD(Filesystem);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Inode
////////////////////////////////////////////////////////////////////////////////

enum {
    kFilePermission_Read = 0x04,
    kFilePermission_Write = 0x02,
    kFilePermission_Execute = 0x01
};

enum {
    kFilePermissionScope_Other = 16,
    kFilePermissionScope_Group = 8,
    kFilePermissionScope_User = 0,

    kFilePermissionScope_Mask = 0x00ff,
    kFilePermissionScope_BitWidth = 8
};

#define FilePermissions_Make(__other, __group, __user) \
  (((__other) & kFilePermissionScope_Mask) << kFilePermissionScope_Other) \
| (((__group) & kFilePermissionScope_Mask) << kFilePermissionScope_Group) \
| (((__user) & kFilePermissionScope_Mask) << kFilePermissionScope_User)

#define FilePermissions_Get(__permissions, __scope) \
(((__permissions) >> (__scope)) & kFilePermissionScope_Mask)

#define FilePermissions_Set(__permissions, __scope, __bits) \
((__permissions) & ~(kFilePermissionsScope_Mask << (__scope))) | (((__bits) & kFilePermissionsScope_Mask) << (__scope))


// The Inode type.
typedef enum _InodeType {
    kInode_RegularFile = 0,     // A regular file that stores data
    kInode_Directory,           // A directory which stores information about child nodes
} InodeType;


// An Inode represents the meta information of a file or directory. This is an
// abstract class that must be subclassed and fully implemented by a file system.
OPEN_CLASS_WITH_REF(Inode, Object,
    Int8            type;
    UInt8           flags;
    FilePermissions permissions;
    User            user;
    InodeId         noid;   // Filesystem specific ID of the inode
    FilesystemId    fsid;   // Globally unique ID of the filesystem that owns this node
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
extern ErrorCode Inode_AbstractCreate(ClassRef pClass, Int8 type, InodeId id, FilesystemId fsid, FilePermissions permissions, User user, InodeRef _Nullable * _Nonnull pOutNode);

// Returns the permissions of the node.
#define Inode_GetFilePermissions(__self) \
    ((InodeRef)__self)->permissions

// Returns the user of the node.
#define Inode_GetUser(__self) \
    ((InodeRef)__self)->user

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


//
// The following functions may be used by any code and they are concurrency safe.
//

// Returns the type of the node.
#define Inode_GetType(__self) \
    ((InodeRef)__self)->type

// Returns true if the node is a directory; false otherwise.
#define Inode_IsDirectory(__self) \
    (Inode_GetType(__self) == kInode_Directory)

// Returns the filesystem specific ID of the node.
#define Inode_GetId(__self) \
    ((InodeRef)__self)->noid

// Returns the ID of the filesystem to which this node belongs.
#define Inode_GetFilesystemId(__self) \
    ((InodeRef)__self)->fsid

// Returns a strong reference to the filesystem that owns the given note. Returns
// NULL if the filesystem isn't mounted.
extern FilesystemRef Inode_CopyFilesystem(InodeRef _Nonnull self);

// Returns true if the receiver and 'pOther' are the same node; false otherwise
extern Bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Path Component
////////////////////////////////////////////////////////////////////////////////

// Describes a single component (name) of a path. A path is a sequence of path
// components separated by a '/' character. Note that a path component is not a
// NUL terminated string. The length of the component is given explicitly by the
// count field.
typedef struct _PathComponent {
    const Character* _Nonnull   name;
    ByteCount                   count;
} PathComponent;

// Mutable version of PathComponent. 'count' must be set on return to the actual
// length of the generated/edited path component. 'capacity' is the maximum length
// that the path component may take on.
typedef struct _MutablePathComponent {
    Character* _Nonnull name;
    ByteCount           count;
    ByteCount           capacity;
} MutablePathComponent;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// A file system stores Inodes. The Inodes may form a tree. This is an abstract
// base  class that must be subclassed and fully implemented by a file system.
//
// Locking protocol
//
// A filesystem is responsible for implementing a locking protocol for the inodes
// that it owns and manages. The filesystem implementor can implement any of the
// following locking models:
//
// a) Single lock: one lock maintained by the filesystem object which protects
//                 the filesystem proper state and all inodes manages by the
//                 filesystem. This is the simplest and least efficient model.
//
// b) Fine-grained locking: the filesystem maintains separate locks for itself
//                          and every single inode that it owns. Eg it adds a
//                          lock to its Inode subclass and one lock to its
//                          Filesystem subclass. Most complicated and efficient
//                          model.
//
// c) Medium-grained locking: the filesystem maintains a set of locks for its
//                            inodes. It partitions its inodes (eg by inode id)
//                            and assigns one lock to each partition. Less resource
//                            use compared to (b).
//
OPEN_CLASS(Filesystem, IOResource,
    FilesystemId        fsid;
);
typedef struct _FilesystemMethodTable {
    IOResourceMethodTable   super;

    //
    // Mounting/Unmounting
    //

    // Invoked when an instance of this file system is mounted.
    ErrorCode (*onMount)(void* _Nonnull self, const Byte* _Nonnull pParams, ByteCount paramsSize);

    // Invoked when a mounted instance of this file system is unmounted. A file
    // system may return an error. Note however that this error is purely advisory
    // and the file system implementation is required to do everything it can to
    // successfully unmount. Unmount errors are ignored and the file system manager
    // will complete the unmount in any case.
    ErrorCode (*onUnmount)(void* _Nonnull self);


    //
    // Filesystem Navigation
    //

    // Returns a strong reference to the root directory node of the filesystem.
    InodeRef _Nonnull (*copyRootNode)(void* _Nonnull self);

    // Returns EOK and the parent node of the given node if it exists and ENOENT
    // and NULL if the given node is the root node of the namespace. This function
    // will always be called with a node that is owned by the file system.
    ErrorCode (*copyParentOfNode)(void* _Nonnull self, InodeRef _Nonnull pNode, User user, InodeRef _Nullable * _Nonnull pOutNode);

    // Returns EOK and the node that corresponds to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function will always only be called with proper node names. Eg never with
    // "." nor "..". If the path component name is longer than what is supported
    // by the file system, ENAMETOOLONG should be returned.
    ErrorCode (*copyNodeForName)(void* _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* pComponent, User user, InodeRef _Nullable * _Nonnull pOutNode);

    // Returns the name of the node 'pNode' which a child of the directory node
    // 'pParentNode'. 'pNode' may be of any type. The name is returned in the
    // mutable path component 'pComponent'. 'count' in path component is 0 on
    // entry and should be set to the actual length of the name on exit. The
    // function is expected to return EOK if the parent node contains 'pNode'
    // and ENOENT otherwise. If the name of 'pNode' as stored in the file system
    // is > the capacity of the path component, then ERANGE should be returned.
    ErrorCode (*getNameOfNode)(void* _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, User user, MutablePathComponent* _Nonnull pComponent);


    //
    // Get/Set Inode Attributes
    //

    // If the node is a directory and another file system is mounted at this directory,
    // then this function returns the filesystem ID of the mounted directory; otherwise
    // 0 is returned.
    FilesystemId (*getFilesystemMountedOnNode)(void* _Nonnull self, InodeRef _Nonnull pNode);

    // Marks the given directory node as a mount point at which the filesystem
    // with the given filesystem ID is mounted. Converts the node back into a
    // regular directory node if the give filesystem ID is 0.
    void (*setFilesystemMountedOnNode)(void* _Nonnull self, InodeRef _Nonnull pNode, FilesystemId fsid);

} FilesystemMethodTable;


// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
extern ErrorCode Filesystem_Create(ClassRef pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys);

// Returns the filesystem ID of the given filesystem.
#define Filesystem_GetId(__fs) \
    ((FilesystemRef)(__fs))->fsid

#define Filesystem_OnMount(__self, __pParams, __paramsSize) \
Object_InvokeN(onMount, Filesystem, __self, __pParams, __paramsSize)

#define Filesystem_OnUnmount(__self) \
Object_Invoke0(onUnmount, Filesystem, __self)

#define Filesystem_CopyRootNode(__self) \
Object_Invoke0(copyRootNode, Filesystem, __self)

#define Filesystem_CopyParentOfNode(__self, __pNode, __user, __pOutNode) \
Object_InvokeN(copyParentOfNode, Filesystem, __self, __pNode, __user, __pOutNode)

#define Filesystem_CopyNodeForName(__self, __pParentNode, __pComponent, __user, __pOutNode) \
Object_InvokeN(copyNodeForName, Filesystem, __self, __pParentNode, __pComponent, __user, __pOutNode)

#define Filesystem_GetNameOfNode(__self, __pParentNode, __pNode, __user, __pComponent) \
Object_InvokeN(getNameOfNode, Filesystem, __self, __pParentNode, __pNode, __user, __pComponent)

#define Filesystem_GetFilesystemMountedOnNode(__self, __pNode) \
Object_InvokeN(getFilesystemMountedOnNode, Filesystem, __self, __pNode)

#define Filesystem_SetFilesystemMountedOnNode(__self, __pNode, __fsid) \
Object_InvokeN(setFilesystemMountedOnNode, Filesystem, __self, __pNode, __fsid)

#endif /* Filesystem_h */
