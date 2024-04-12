//
//  Filesystem.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Filesystem_h
#define Filesystem_h

#include <driver/DiskDriver.h>
#include "Inode.h"
#include "PathComponent.h"
#include "User.h"


// A file system stores Inodes. The Inodes may form a tree. This is an abstract
// base  class that must be subclassed and fully implemented by a file system.
//
// Inode state
//
// It is the responsibility of the filesystem to provide the required functionality
// to modify the state of inodes and doing it in a way that preserves consistency
// in the face on concurrency. Thus it is the job of the filesystem to implement
// and apply a locking model for inodes. See "Locking protocol" below.
// 
// Locking protocol
//
// Every inode has a lock associated with it. The filesystem (XXX currently)
// must lock the inode before it accesses or modifies any of its properties. 
//
open_class(Filesystem, Object,
    FilesystemId        fsid;
    Lock                inodeManagementLock;
    PointerArray        inodesInUse;
);
typedef struct FilesystemMethodTable {
    ObjectMethodTable   super;

    //
    // Mounting/Unmounting
    //

    // Invoked when an instance of this file system is mounted. Note that the
    // kernel guarantees that no operations will be issued to the filesystem
    // before onMount() has returned with EOK.
    errno_t (*onMount)(void* _Nonnull self, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize);

    // Invoked when a mounted instance of this file system is unmounted. A file
    // system may return an error. Note however that this error is purely advisory
    // and the file system implementation is required to do everything it can to
    // successfully unmount. Unmount errors are ignored and the file system manager
    // will complete the unmount in any case.
    errno_t (*onUnmount)(void* _Nonnull self);


    //
    // Filesystem Navigation
    //

    // Returns the root node of the filesystem if the filesystem is currently in
    // mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
    errno_t (*acquireRootNode)(void* _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode);

    // Returns EOK and the node that corresponds to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function has to support the special names "." (node itself) and ".."
    // (parent of node) in addition to "regular" filenames. If 'pParentNode' is
    // the root node of the filesystem and 'pComponent' is ".." then 'pParentNode'
    // should be returned. If the path component name is longer than what is
    // supported by the file system, ENAMETOOLONG should be returned.
    errno_t (*acquireNodeForName)(void* _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable _Locked * _Nonnull pOutNode);

    // Returns the name of the node with the id 'id' which a child of the
    // directory node 'pParentNode'. 'id' may be of any type. The name is
    // returned in the mutable path component 'pComponent'. 'count' in path
    // component is 0 on entry and should be set to the actual length of the
    // name on exit. The function is expected to return EOK if the parent node
    // contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
    // file system is > the capacity of the path component, then ERANGE should
    // be returned.
    errno_t (*getNameOfNode)(void* _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent);


    //
    // Get/Set Inode Attributes
    //

    // Returns a file info record for the given Inode. The Inode may be of any
    // file type.
    errno_t (*getFileInfo)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo);

    // Modifies one or more attributes stored in the file info record of the given
    // Inode. The Inode may be of any type.
    errno_t (*setFileInfo)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo);


    //
    // File Specific Operations
    //

    // Creates and opens a file and returns the inode of that file. The behavior is
    // non-exclusive by default. Meaning the file is created if it does not 
    // exist and the file's inode is merrily acquired if it already exists. If
    // the mode is exclusive then the file is created if it doesn't exist and
    // an error is thrown if the file exists. Returns a file object, representing
    // the created and opened file.
    errno_t (*createFile)(void* _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, unsigned int options, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);

    // Opens the file identified by the given inode. The file is opened for
    // reading and or writing, depending on the 'mode' bits.
    errno_t (*openFile)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, unsigned int mode, User user);

    // Reads up to 'nBytesToRead' bytes starting at the file offset 'pInOutOffset'
    // from the file 'pNode'.
    errno_t (*readFile)(void* _Nonnull self, InodeRef _Nonnull pNode, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead);

    // Writes up to 'nBytesToWrite' bytes starting at file offset 'pInOutOffset'
    // to the file 'pNode'.
    errno_t (*writeFile)(void* _Nonnull self, InodeRef _Nonnull pNode, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesWritten);


    //
    // Directory Specific Operations
    //

    // Creates an empty directory as a child of the given directory node and with
    // the given name, user and file permissions. Returns EEXIST if a node with
    // the given name already exists.
    errno_t (*createDirectory)(void* _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions);

    // Opens the directory represented by the given node. The filesystem is
    // expected to validate whether the user has access to the directory content.
    errno_t (*openDirectory)(void* _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user);

    // Reads the next set of directory entries. The first entry read is the one
    // at the current directory index stored in 'pDir'. This function guarantees
    // that it will only ever return complete directories entries. It will never
    // return a partial entry. Consequently the provided buffer must be big enough
    // to hold at least one directory entry. Note that this function is expected
    // to return "." for the entry at index #0 and ".." for the entry at index #1.
    errno_t (*readDirectory)(void* _Nonnull self, InodeRef _Nonnull pDirNode, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead);


    //
    // General File/Directory Operations
    //

    // Verifies that the given node is accessible assuming the given access mode.
    errno_t (*checkAccess)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, AccessMode mode);

    // Change the size of the file 'pNode' to 'length'. EINVAL is returned if
    // the new length is negative. No longer needed blocks are deallocated if
    // the new length is less than the old length and zero-fille blocks are
    // allocated and assigned to the file if the new length is longer than the
    // old length. Note that a filesystem implementation is free to defer the
    // actual allocation of the new blocks until an attempt is made to read or
    // write them.
    errno_t (*truncate)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FileOffset length);

    // Unlink the node 'pNode' which is an immediate child of 'pParentNode'.
    // Both nodes are guaranteed to be members of the same filesystem. 'pNode'
    // is guaranteed to exist and that it isn't a mountpoint and not the root
    // node of the filesystem.
    // This function must validate that that if 'pNode' is a directory, that the
    // directory is empty (contains nothing except "." and "..").
    errno_t (*unlink)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeRef _Nonnull _Locked pParentNode, User user);

    // Renames the node with name 'pName' and which is an immediate child of the
    // node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
    // the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
    errno_t (*rename)(void* _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pNewParentNode, User user);


    //
    // Required override points for subclassers
    //

    // Invoked when Filesystem_AcquireNodeWithId() needs to read the requested
    // inode off the disk. The override should read the inode data from the disk,
    // create and inode instance and fill it in with the data from the disk and
    // then return it. It should return a suitable error and NULL if the inode
    // data can not be read off the disk.
    errno_t (*onReadNodeFromDisk)(void* _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode);

    // Invoked when the inode is relinquished and it is marked as modified. The
    // filesystem override should write the inode meta-data back to the 
    // corresponding disk node.
    errno_t (*onWriteNodeToDisk)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode);

    // Invoked when Filesystem_RelinquishNode() has determined that the inode is
    // no longer being referenced by any directory and that the on-disk
    // representation should be deleted from the disk and deallocated. This
    // operation is assumed to never fail.
    void (*onRemoveNodeFromDisk)(void* _Nonnull self, InodeRef _Nonnull pNode);

} FilesystemMethodTable;


//
// Methods for use by filesystem users.
//

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
extern errno_t Filesystem_Create(ClassRef pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys);

// Returns the filesystem ID of the given filesystem.
#define Filesystem_GetId(__fs) \
    ((FilesystemRef)(__fs))->fsid

#define Filesystem_OnMount(__self, __pDriver, __pParams, __paramsSize) \
Object_InvokeN(onMount, Filesystem, __self, __pDriver, __pParams, __paramsSize)

#define Filesystem_OnUnmount(__self) \
Object_Invoke0(onUnmount, Filesystem, __self)


#define Filesystem_AcquireRootNode(__self, __pOutNode) \
Object_InvokeN(acquireRootNode, Filesystem, __self, __pOutNode)

#define Filesystem_AcquireNodeForName(__self, __pParentNode, __pComponent, __user, __pOutNode) \
Object_InvokeN(acquireNodeForName, Filesystem, __self, __pParentNode, __pComponent, __user, __pOutNode)

#define Filesystem_GetNameOfNode(__self, __pParentNode, __id, __user, __pComponent) \
Object_InvokeN(getNameOfNode, Filesystem, __self, __pParentNode, __id, __user, __pComponent)


#define Filesystem_GetFileInfo(__self, __pNode, __pOutInfo) \
Object_InvokeN(getFileInfo, Filesystem, __self, __pNode, __pOutInfo)

#define Filesystem_SetFileInfo(__self, __pNode, __user, __pInfo) \
Object_InvokeN(setFileInfo, Filesystem, __self, __pNode, __user, __pInfo)


#define Filesystem_CreateFile(__self, __pName, __pParentNode, __user, __options, __permissions, __pOutNode) \
Object_InvokeN(createFile, Filesystem, __self, __pName, __pParentNode, __user, __options, __permissions, __pOutNode)

#define Filesystem_OpenFile(__self, __pNode, __mode, __user) \
Object_InvokeN(openFile, Filesystem, __self, __pNode, __mode, __user)

#define Filesystem_ReadFile(__self, __pNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead) \
Object_InvokeN(readFile, Filesystem, __self, __pNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead)

#define Filesystem_WriteFile(__self, __pNode, __pBuffer, __nBytesToWrite, __pInOutOffset, __nOutBytesWritten) \
Object_InvokeN(writeFile, Filesystem, __self, __pNode, __pBuffer, __nBytesToWrite, __pInOutOffset, __nOutBytesWritten)


#define Filesystem_CreateDirectory(__self, __pName, __pParentNode, __user, __permissions) \
Object_InvokeN(createDirectory, Filesystem, __self, __pName, __pParentNode, __user, __permissions)

#define Filesystem_OpenDirectory(__self, __pDirNode, __user) \
Object_InvokeN(openDirectory, Filesystem, __self, __pDirNode, __user)

#define Filesystem_ReadDirectory(__self, __pDirNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead) \
Object_InvokeN(readDirectory, Filesystem, __self, __pDirNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead)


#define Filesystem_CheckAccess(__self, __pNode, __user, __mode) \
Object_InvokeN(checkAccess, Filesystem, __self, __pNode, __user, __mode)

#define Filesystem_Truncate(__self, __pNode, __user, __length) \
Object_InvokeN(truncate, Filesystem, __self, __pNode, __user, __length)

#define Filesystem_Unlink(__self, __pNode, __pParentNode, __user) \
Object_InvokeN(unlink, Filesystem, __self, __pNode, __pParentNode, __user)

#define Filesystem_Rename(__self, __pName, __pParentNode, __pNewName, __pNewParentNode, __user) \
Object_InvokeN(rename, Filesystem, __self, __pName, __pParentNode, __pNewName, __pNewParentNode, __user)

// Acquires a new reference to the given node. The returned node is locked.
extern InodeRef _Nonnull _Locked Filesystem_ReacquireNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode);

// Acquires a new reference to the given node. The returned node is NOT locked.
extern InodeRef _Nonnull Filesystem_ReacquireUnlockedNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode);

// Relinquishes the given node back to the filesystem. This method will invoke
// the filesystem onRemoveNodeFromDisk() if no directory is referencing the inode
// anymore. This will remove the inode from disk.
extern void Filesystem_RelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nullable _Locked pNode);


//
// Methods for use by filesystem subclassers.
//

// Publishes the given inode. Publishing should be the last step in creating a
// new inode in order to make it visible and accessible to other part of the
// kernel. Note that the inode that is passed to this function must not have
// been acquired via Filesystem_AcquireNode(). The passed in inode must be a
// freshly allocated inode (via Inode_Create). The inode is considered acquired
// once this function returns. This means that you have to relinquish it by
// calling Filesystem_RelinquishNode(). 
extern errno_t Filesystem_PublishNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode);

// Acquires the inode with the ID 'id'. The node is returned in a locked state.
// This methods guarantees that there will always only be at most one inode instance
// in memory at any given time and that only one VP can access/modify the inode.
// Once you're done with the inode, you should relinquish it back to the filesystem.
// This method should be used by subclassers to acquire inodes in order to return
// them to a filesystem user.
// This method calls the filesystem method onReadNodeFromDisk() to read the
// requested inode off the disk if there is no inode instance in memory at the
// time this method is called.
// \param self the filesystem instance
// \param id the id of the inode to acquire
// \param pOutNode receives the acquired inode
extern errno_t Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, InodeId id, InodeRef _Nullable _Locked * _Nonnull pOutNode);

// Returns true if the filesystem can be safely unmounted which means that no
// inodes owned by the filesystem is currently in memory.
extern bool Filesystem_CanSafelyUnmount(FilesystemRef _Nonnull self);

#define Filesystem_OnReadNodeFromDisk(__self, __id, __pOutNode) \
Object_InvokeN(onReadNodeFromDisk, Filesystem, __self, __id, __pOutNode)

#define Filesystem_OnWriteNodeToDisk(__self, __pNode) \
Object_InvokeN(onWriteNodeToDisk, Filesystem, __self, __pNode)

#define Filesystem_OnRemoveNodeFromDisk(__self, __pNode) \
Object_InvokeN(onRemoveNodeFromDisk, Filesystem, __self, __pNode)

#endif /* Filesystem_h */
