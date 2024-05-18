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


// Can be used by filesystem subclasses to temporarily store a hint where a new
// directory entry may be inserted in a directory structure. This hint is returned
// by the AcquireNodeForName() function and it is passed to the CreateNode()
// function. This structure is able to store 32 bytes and guarantees that the
// data is aligned suitably for 64bit words.
typedef struct DirectoryEntryInsertionHint {
    uint64_t    data[4];
} DirectoryEntryInsertionHint;


// A filesystem stores files and directories persistently. Files and directories
// are represented in memory by Inode instances. An inode has to be acquired by
// invoking one of the Filesystem_AcquireNode() functions before it can be used.
// Once an inode is no longer needed, it should be relinquished which will cause
// the filesystem to write inode metadata changes back to the underlying storage
// device.
//
//
// Filesystem and inode lifetimes:
//
// The lifetime of a filesystem instance is always >= the lifetime of all
// acquired inodes. This is guaranteed by ensuring that a filesystem can not be
// destroyed as long as there is at least one acquired inode outstanding. Thus
// it is sufficient to either hold a strong reference to a filesystem object
// (use Object_Retain() to get it) or a strong reference to an inode from the
// filesystem in question (use Filesystem_AcquireNode9) to get it) to ensure that
// the filesystem stays alive.
//
//
// Mounting/unmounting a filesystem:
//
// A filesystem must be mounted before it can be used and any inodes can be
// acquired. Conversely all acquired inodes must have been relinquished before
// the filesystem can be unmounted and destroyed. However a filesystem may be
// force-unmounted which means that the filesystem is removed from the file
// hierarchy (and thus is no longer accessible by any process) and the actual
// unmount and destruction action is deferred until the last acquired inode is
// relinquished.
// Note that a particular filesystem instance can be mounted at most once at any
// given time.
//
//
// Locking protocol(s):
//
// File & Directory I/O Channels:
//
// This is handled by the kernel. Files and directories serialize read/write/seek
// operations to ensure that a read will return all the original data found in the
// region that it accesses and not a mix of original data and data that a concurrent
// write wants to place there. Additionally this serialization ensures that the
// current file position moves in a meaningful way and not in a way where it
// appears to erratically jump forward and backward between concurrently scheduled
// operations.
//
// Inode Acquisition, Relinquishing, Write-Backs and On-Disk Removal:
//
// This is handled by the Filesystem base class. Inode acquisition and
// relinquishing is protected by an inode management lock and these operations
// are atomic. Furthermore writing the changed (meta-)data of an inode back to
// disk and deleting the the (meta-)data of an inode on disk are done atomically
// in the sense that no other thread of execution can acquire an inode that is
// currently in the process of a write-back or on-disk removal.
// Reacquiring an inode is also atomic. Finally publishing an inode is an atomic
// operation that guarantees that the inode to publish will only be accessible
// to other threads of execution (via acquisition) once the publish operation has
// completed.
//
// Filesystem Mount, Unmount and Root Node Acquisition:
//
// This must be implemented by Filesystem subclassers. A concrete Filesystem
// implementation must guarantee that mount, unmount and root inode acquisition
// operations are executed non-currently and atomically. Ie all three operations
// and their shared state must be protected by a lock. This way a filesystem user
// can be sure that once they have successfully acquired the root node of a
// filesystem that the filesystem can not be unmounted and neither deallocated
// for as long as the user is holding on to the inode and using it. An unmount
// will only succeed if no inodes are acquired at the beginning of the unmount
// operation. Furthermore a Filesystem subclass implementation must guarantee
// that a root inode acquisition will fail with an EIO error if the filesystem
// is in unmounted state.
//
// Remember that a filesystem can not be unmounted and neither deallocated as
// long as there is at least one acquired inode outstanding. Because of this and
// the fact that all filesystem operations expect at least one inode as input,
// non of the filesystem operation functions need to be protected with a lock.
// The inode that is passed to an operation acts in a sense as a lock and a
// guarantee that the filesystem can not be unmounted and deallocated while the
// operation is executing.
//
open_class(Filesystem, Object,
    FilesystemId        fsid;
    Lock                inodeManagementLock;
    PointerArray        inodesInUse;
);
open_class_funcs(Filesystem, Object,

    //
    // Mounting/Unmounting
    //

    // Invoked when an instance of this file system is mounted. Note that the
    // kernel guarantees that no operations will be issued to the filesystem
    // before onMount() has returned with EOK.
    // Override: Advised
    // Default Behavior: Returns EIO
    errno_t (*onMount)(void* _Nonnull self, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize);

    // Invoked when a mounted instance of this file system is unmounted. A file
    // system may return an error. Note however that this error is purely advisory
    // and the file system implementation is required to do everything it can to
    // successfully unmount. Unmount errors are ignored and the file system manager
    // will complete the unmount in any case.
    // Override: Optional
    // Default Behavior: Returns EOK
    errno_t (*onUnmount)(void* _Nonnull self);


    //
    // Filesystem Navigation
    //

    // Returns the root node of the filesystem if the filesystem is currently in
    // mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
    // Override: Advised
    // Default Behavior: Returns NULL and EIO
    errno_t (*acquireRootNode)(void* _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode);

    // Returns EOK and the node that corresponds to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function has to support the special name ".." (parent of node) in addition
    // to "regular" filenames. If 'pParentNode' is the root node of the filesystem
    // and 'pComponent' is ".." then 'pParentNode' should be returned. Note that
    // a lookup of '..' may not fail with ENOENT. This particular kind of lookup
    // must always succeed or fail with a general I/O error. If the path component
    // name is longer than what is supported by the file system, ENAMETOOLONG
    // should be returned.  caller may pass a pointer to a directory-entry-insertion-
    // hint data structure. This function may store information in this data
    // structure to help speed up a follow=up CreateNode() call for a node with
    // the name 'pComponent' in the directory 'pParentNode'.
    // Override: Advised
    // Default Behavior: Returns NULL and ENOENT (EIO for '..' lookups)
    errno_t (*acquireNodeForName)(void* _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pComponent, User user, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable _Locked * _Nonnull pOutNode);

    // Returns the name of the node with the id 'id' which a child of the
    // directory node 'pParentNode'. 'id' may be of any type. The name is
    // returned in the mutable path component 'pComponent'. 'count' in path
    // component is 0 on entry and should be set to the actual length of the
    // name on exit. The function is expected to return EOK if the parent node
    // contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
    // file system is > the capacity of the path component, then ERANGE should
    // be returned.
    // Override: Advised
    // Default Behavior: Returns EIO and sets 'pComponent' to an empty name
    errno_t (*getNameOfNode)(void* _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent);


    //
    // Get/Set Inode Attributes
    //

    // Returns a file info record for the given Inode. The Inode may be of any
    // type.
    // Override: Optional
    // Default Behavior: Returns the inode's file info
    errno_t (*getFileInfo)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo);

    // Modifies one or more attributes stored in the file info record of the given
    // Inode. The Inode may be of any type.
    // Override: Optional
    // Default Behavior: Updates the inode's file info
    errno_t (*setFileInfo)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo);


    //
    // File Specific Operations
    //

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

    // Creates a new inode with type 'type', user information 'user', permissions
    // 'permissions' and adds it to parent inode (directory) 'pParentNode'. The new
    // node will be added to 'pParentNode' with the name 'pName'. Returns the newly
    // acquired inode on success and NULL otherwise.
    errno_t (*createNode)(void* _Nonnull self, FileType type, User user, FilePermissions permissions, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsertionHint, InodeRef _Nullable * _Nonnull pOutNode);

    // Verifies that the given node is accessible assuming the given access mode.
    // Override: Optional
    // Default Behavior: Returns EOK if the given node is accessible; EACCESS or EROFS otherwise
    errno_t (*checkAccess)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, AccessMode mode);

    // Change the size of the file 'pNode' to 'length'. EINVAL is returned if
    // the new length is negative. No longer needed blocks are deallocated if
    // the new length is less than the old length and zero-fille blocks are
    // allocated and assigned to the file if the new length is longer than the
    // old length. Note that a filesystem implementation is free to defer the
    // actual allocation of the new blocks until an attempt is made to read or
    // write them.
    errno_t (*truncate)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FileOffset length);

    // Unlink the node 'pNode' which is an immediate child of 'pParentDir'.
    // Both nodes are guaranteed to be members of the same filesystem. 'pNode'
    // is guaranteed to exist and that it isn't a mountpoint and not the root
    // node of the filesystem.
    // This function must validate that that if 'pNode' is a directory, that the
    // directory is empty (contains nothing except "." and "..").
    errno_t (*unlink)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeRef _Nonnull _Locked pParentDir, User user);

    // Renames the node 'pSourceNode' which is an immediate child of the
    // node 'pSourceDir' such that it becomes a child of 'pTargetDir' with
    // the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
    errno_t (*rename)(void* _Nonnull self, InodeRef _Nonnull pSourceNode, InodeRef _Nonnull _Locked pSourceDir, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pTargetDir, User user);


    //
    // Subclassing Override points
    //

    // Invoked when Filesystem_AcquireNodeWithId() needs to read the requested
    // inode off the disk. The override should read the inode data from the disk,
    // create and inode instance and fill it in with the data from the disk and
    // then return it. It should return a suitable error and NULL if the inode
    // data can not be read off the disk.
    // Override: Advised
    // Default Behavior: Returns EIO
    errno_t (*onReadNodeFromDisk)(void* _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode);

    // Invoked when the inode is relinquished and it is marked as modified. The
    // filesystem override should write the inode meta-data back to the 
    // corresponding disk node.
    // Override: Advised
    // Default Behavior: Returns EIO
    errno_t (*onWriteNodeToDisk)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode);

    // Invoked when Filesystem_RelinquishNode() has determined that the inode is
    // no longer being referenced by any directory and that the on-disk
    // representation should be deleted from the disk and deallocated. This
    // operation is assumed to never fail.
    // Override: Advised
    // Default Behavior: Does nothing
    void (*onRemoveNodeFromDisk)(void* _Nonnull self, InodeRef _Nonnull pNode);

    // Returns a set of file permissions that apply to all files of type 'fileType'
    // on the disk. Ie if a filesystem supports a read-only mounting option then
    // this function should return 0555. If the filesystem supports a do-not-
    // execute-files mount option then this function should return 0666. A
    // filesystem which always supports all permissions for all file types and
    // permission classes should return 0777 (this is what the default
    // implementation does).
    // Override: Optional
    // Default Behavior: Returns 0777 (R/W/X for user, group & other)
    FilePermissions (*getDiskPermissions)(void* _Nonnull self, FileType fileType);
);


//
// Methods for use by filesystem users.
//

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
extern errno_t Filesystem_Create(Class* pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys);

// Returns the filesystem ID of the given filesystem.
#define Filesystem_GetId(__fs) \
    ((FilesystemRef)(__fs))->fsid

#define Filesystem_OnMount(__self, __pDriver, __pParams, __paramsSize) \
invoke_n(onMount, Filesystem, __self, __pDriver, __pParams, __paramsSize)

#define Filesystem_OnUnmount(__self) \
invoke_0(onUnmount, Filesystem, __self)


#define Filesystem_AcquireRootNode(__self, __pOutNode) \
invoke_n(acquireRootNode, Filesystem, __self, __pOutNode)

#define Filesystem_AcquireNodeForName(__self, __pParentNode, __pComponent, __user, __pDirInsHint, __pOutNode) \
invoke_n(acquireNodeForName, Filesystem, __self, __pParentNode, __pComponent, __user, __pDirInsHint, __pOutNode)

#define Filesystem_GetNameOfNode(__self, __pParentNode, __id, __user, __pComponent) \
invoke_n(getNameOfNode, Filesystem, __self, __pParentNode, __id, __user, __pComponent)


#define Filesystem_CreateNode(__self, __type, __user, __permissions, __pParentNode, __pName, __pDirInsertionHint, __pOutNode) \
invoke_n(createNode, Filesystem, __self, __type, __user, __permissions, __pParentNode, __pName, __pDirInsertionHint, __pOutNode)


#define Filesystem_GetFileInfo(__self, __pNode, __pOutInfo) \
invoke_n(getFileInfo, Filesystem, __self, __pNode, __pOutInfo)

#define Filesystem_SetFileInfo(__self, __pNode, __user, __pInfo) \
invoke_n(setFileInfo, Filesystem, __self, __pNode, __user, __pInfo)


#define Filesystem_OpenFile(__self, __pNode, __mode, __user) \
invoke_n(openFile, Filesystem, __self, __pNode, __mode, __user)

#define Filesystem_ReadFile(__self, __pNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead) \
invoke_n(readFile, Filesystem, __self, __pNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead)

#define Filesystem_WriteFile(__self, __pNode, __pBuffer, __nBytesToWrite, __pInOutOffset, __nOutBytesWritten) \
invoke_n(writeFile, Filesystem, __self, __pNode, __pBuffer, __nBytesToWrite, __pInOutOffset, __nOutBytesWritten)


#define Filesystem_OpenDirectory(__self, __pDirNode, __user) \
invoke_n(openDirectory, Filesystem, __self, __pDirNode, __user)

#define Filesystem_ReadDirectory(__self, __pDirNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead) \
invoke_n(readDirectory, Filesystem, __self, __pDirNode, __pBuffer, __nBytesToRead, __pInOutOffset, __nOutBytesRead)


#define Filesystem_CheckAccess(__self, __pNode, __user, __mode) \
invoke_n(checkAccess, Filesystem, __self, __pNode, __user, __mode)

#define Filesystem_Truncate(__self, __pNode, __user, __length) \
invoke_n(truncate, Filesystem, __self, __pNode, __user, __length)

#define Filesystem_Unlink(__self, __pNode, __pParentDir, __user) \
invoke_n(unlink, Filesystem, __self, __pNode, __pParentDir, __user)

#define Filesystem_Rename(__self, __pSourceNode, __pSourceDir, __pNewName, __pTargetDir, __user) \
invoke_n(rename, Filesystem, __self, __pSourceNode, __pSourceDir, __pNewName, __pTargetDir, __user)

// Acquires a new reference to the given node.
extern InodeRef _Nonnull _Locked Filesystem_ReacquireNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode);

// Relinquishes the given node back to the filesystem. This method will invoke
// the filesystem onRemoveNodeFromDisk() if no directory is referencing the inode
// anymore. This will remove the inode from disk.
extern errno_t Filesystem_RelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nullable _Locked pNode);


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

// Acquires the inode with the ID 'id'. This methods guarantees that there will
// always only be at most one inode instance in memory at any given time and
// that only one VP can access/modify the inode.
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

// Returns true if the filesystem can be unmounted which means that there are no
// acquired inodes outstanding that belong to this filesystem.
extern bool Filesystem_CanUnmount(FilesystemRef _Nonnull self);

#define Filesystem_OnReadNodeFromDisk(__self, __id, __pOutNode) \
invoke_n(onReadNodeFromDisk, Filesystem, __self, __id, __pOutNode)

#define Filesystem_OnWriteNodeToDisk(__self, __pNode) \
invoke_n(onWriteNodeToDisk, Filesystem, __self, __pNode)

#define Filesystem_OnRemoveNodeFromDisk(__self, __pNode) \
invoke_n(onRemoveNodeFromDisk, Filesystem, __self, __pNode)

#define Filesystem_GetDiskPermissions(__self, __fileType) \
invoke_n(getDiskPermissions, Filesystem, __self, __fileType)

#endif /* Filesystem_h */
