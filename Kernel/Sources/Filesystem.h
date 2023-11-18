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


#define IREAD   0x0004
#define IWRITE  0x0002
#define IEXEC   0x0001

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
    UInt16          permissions;
    UserId          uid;
    GroupId         gid;
    FilesystemId    fsid;   // ID of the filesystem that owns this node
    union {
        FilesystemId    mountedFileSys;     // (Directory) The ID of the filesystem mounted on top of this directory; 0 if this directory is not a mount point
    }               u;
);
typedef struct _InodeMethodTable {
    ObjectMethodTable   super;
} InodeMethodTable;


// Creates an instance of the abstract Inode class. Should only ever be called
// by the implement of a creation function for a concrete Inode subclass.
extern ErrorCode Inode_AbstractCreate(ClassRef pClass, Int8 type, FilesystemId fsid, InodeRef _Nullable * _Nonnull pOutNode);

// Returns the type of the node.
#define Inode_GetType(__self) \
    ((InodeRef)__self)->type

// Returns true if the node is a directory; false otherwise.
#define Inode_IsDirectory(__self) \
    (Inode_GetType(__self) == kInode_Directory)

// Returns the permissions of the node.
#define Inode_GetPermissions(__self) \
    ((InodeRef)__self)->permissions

// Returns the User ID of the node.
#define Inode_GetUserId(__self) \
    ((InodeRef)__self)->uid

// Returns the group ID of the node.
#define Inode_GetGroupId(__self) \
    ((InodeRef)__self)->gid

// Returns the ID of the filesystem to which this node belongs.
#define Inode_GetFilesystemId(__self) \
    ((InodeRef)__self)->fsid

// Returns a strong reference to the filesystem that owns the given note. Returns
// NULL if the filesystem isn't mounted.
extern FilesystemRef Inode_CopyFilesystem(InodeRef _Nonnull pNode);

// If the node is a directory and another file system is mounted at this directory,
// then this function returns the filesystem ID of the mounted directory; otherwise
// 0 is returned.
extern FilesystemId Inode_GetMountedFilesystemId(InodeRef _Nonnull pNode);

// Marks the given node as a mount point at which the filesystem with the given
// filesystem ID is mounted. Converts the node back into a regular directory
// node if the give filesystem ID is 0.
extern void Inode_SetMountedFilesystemId(InodeRef _Nonnull pNode, FilesystemId fsid);

// Returns true if the receiver is a child node of 'pOtherNode' or it is 'pOtherNode';
// otherwise returns false. An Error is returned if the relationship can not be
// successful established because eg the function detects that the node or one of
// its parents is owned by a file system that is not currently mounted or because
// of a lack of permissions.
extern ErrorCode Inode_IsChildOfNode(InodeRef _Nonnull pChildNode, InodeRef _Nonnull pOtherNode, Bool* _Nonnull pOutResult);


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


// A file system stores Inodes. The Inodes may form a tree. This is an abstract
// base  class that must be subclassed and fully implemented by a file system.
OPEN_CLASS(Filesystem, IOResource,
    FilesystemId        fsid;
);
typedef struct _FilesystemMethodTable {
    IOResourceMethodTable   super;

    // Returns a strong reference to the root directory node of the filesystem.
    InodeRef _Nonnull (*copyRootNode)(void* _Nonnull self);

    // Returns EOK and the parent node of the given node if it exists and ENOENT
    // and NULL if the given node is the root node of the namespace. This function
    // will always be called with a node that is owned by the file system.
    ErrorCode (*copyParentOfNode)(void* _Nonnull self, InodeRef _Nonnull pNode, InodeRef _Nullable * _Nonnull pOutNode);

    // Returns EOK and the node that corresponds to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function will always only be called with proper node names. Eg never with
    // "." nor "..". If the path component name is longer than what is supported
    // by the file system, ENAMETOOLONG should be returned.
    ErrorCode (*copyNodeForName)(void* _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* pComponent, InodeRef _Nullable * _Nonnull pOutNode);

    // Returns the name of the node 'pNode' which a child of the directory node
    // 'pParentNode'. 'pNode' may be of any type. The name is returned in the
    // mutable path component 'pComponent'. 'count' in path component is 0 on
    // entry and should be set to the actual length of the name on exit. The
    // function is expected to return EOK if the parent node contains 'pNode'
    // and ENOENT otherwise. If the name of 'pNode' as stored in the file system
    // is > the capacity of the path component, then ERANGE should be returned.
    ErrorCode (*getNameOfNode)(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, MutablePathComponent* _Nonnull pComponent);
} FilesystemMethodTable;


// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
extern ErrorCode Filesystem_Create(ClassRef pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys);

// Returns the filesystem ID of the given filesystem.
#define Filesystem_GetId(__fs) \
    ((FilesystemRef)(__fs))->fsid

#define Filesystem_CopyRootNode(__self) \
Object_Invoke0(copyRootNode, Filesystem, __self)

#define Filesystem_CopyParentOfNode(__self, __pNode, __pOutNode) \
Object_InvokeN(copyParentOfNode, Filesystem, __self, __pNode, __pOutNode)

#define Filesystem_CopyNodeForName(__self, __pParentNode, __pComponent, __pOutNode) \
Object_InvokeN(copyNodeForName, Filesystem, __self, __pParentNode, __pComponent, __pOutNode)

#define Filesystem_GetNameOfNode(__self, __pParentNode, __pNode, __pComponent) \
Object_InvokeN(getNameOfNode, Filesystem, __self, __pParentNode, __pNode, __pComponent)

#endif /* Filesystem_h */
