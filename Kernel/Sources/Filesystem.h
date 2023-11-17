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

typedef enum _InodeType {
    kInode_RegularFile = 0,
    kInode_Directory
} InodeType;


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
    void*           refcon; // Filesystem specific information
);

typedef struct _InodeMethodTable {
    ObjectMethodTable   super;
} InodeMethodTable;


extern ErrorCode Inode_Create(Int8 type, FilesystemId fsid, void* _Nullable refcon, InodeRef _Nullable * _Nonnull pOutInode);
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

// Returns the node's refcon field. The refcon is defined by the filesystem that
// has created and owns the node. The refcon is usually also type specific.
// Filesystem implementor can use this macro to get the refcon. However a user of
// a filesystem should always use the accessor function that the filesystem
// provides to ensure proper memory management (eg ref counting). 
#define Inode_GetRefcon(__self) \
    ((InodeRef)__self)->refcon

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

extern ErrorCode Inode_IsChildOfNode(InodeRef _Nonnull pChildNode, InodeRef _Nonnull pOtherNode, Bool* _Nonnull pOutResult);


////////////////////////////////////////////////////////////////////////////////

typedef struct _PathComponent {
    const Character* _Nonnull   name;
    ByteCount                   count;
} PathComponent;

typedef struct _MutablePathComponent {
    Character* _Nonnull name;
    ByteCount           count;
    ByteCount           capacity;
} MutablePathComponent;


OPEN_CLASS(Filesystem, IOResource,
    FilesystemId        fsid;
);

typedef struct _FilesystemMethodTable {
    IOResourceMethodTable   super;

    // Returns a strong reference to the root directory node of the filesystem.
    InodeRef _Nonnull (*copyRootNode)(void* _Nonnull self);

    // Returns EOK and the parent node of the given node if it exists and ENOENT
    // and NULL if the given node is the root node of the namespace.
    ErrorCode (*copyParentOfNode)(void* _Nonnull self, InodeRef _Nonnull pNode, InodeRef _Nullable * _Nonnull pOutNode);

    // Returns EOK and the node that corresponds to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function will always only be called with proper node names. Eg never with
    // "." nor "..".
    ErrorCode (*copyNodeForName)(void* _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* pComponent, InodeRef _Nullable * _Nonnull pOutNode);

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
