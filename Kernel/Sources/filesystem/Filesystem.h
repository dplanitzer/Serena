//
//  Filesystem.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Filesystem_h
#define Filesystem_h

#include <kobj/Object.h>
#include <klib/List.h>
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
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
// The underlying medium
//
// Every filesystem sits on top of some medium and it is responsible for managing
// the data of this medium. A medium can be anything as far as the abstract
// Filesystem base class is concerned: it may be a physical disk, a tape or maybe
// some kind of object that only exists in memory and does not even persist across
// reboot.
// The ContainerFilesystem subclass is a specialization of Filesystem which builds
// on top of a FSContainer. A FSContainer represents a logical disk which may map
// 1:1 to a single physical disk or an array of physical disks. Concrete filesystem
// implementations which are meant to implement a traditional disk-based filesystem
// should derive from ContainerFilesystem instead of Filesystem directly.
//
//
// Filesystem and inode lifetimes:
//
// The lifetime of a filesystem instance is always >= the lifetime of all
// acquired inodes. This is guaranteed by ensuring that a filesystem can not be
// destroyed as long as there is at least one acquired inode outstanding. Thus
// it is sufficient to either hold a strong reference to a filesystem object
// (use Object_Retain() to get it) or a strong reference to an inode from the
// filesystem in question (use Filesystem_AcquireNode() to get one) to ensure
// that the filesystem stays alive.
//
//
// Starting/stopping a filesystem:
//
// A filesystem must be started before it can be used and any inodes can be
// acquired. Conversely all acquired inodes must have been relinquished before
// the filesystem can be stopped and destroyed. However a filesystem may be
// force-stopped which means that the filesystem is removed from the file
// hierarchy (and thus is no longer accessible by any process) and the actual
// destruction action is deferred until the last acquired inode is relinquished.
// Note that a particular filesystem instance can be started at most once at any
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
    fsid_t              fsid;
    ConditionVariable   inCondVar;
    Lock                inLock;
    List* _Nonnull      inCached;   // <Inode>
    size_t              inCachedCount;
    List* _Nonnull      inReading;  // <RDnode>
    size_t              inReadingCount;
    size_t              inReadingWaiterCount;
    List                inReadingCache;
    size_t              inReadingCacheCount;
);
open_class_funcs(Filesystem, Object,

    //
    // Mounting/Unmounting
    //

    // Invoked when an instance of this file system is mounted. Overrides of this
    // method should read the root information off the underlying medium and
    // prepare the filesystem for use. Ie it must be possible to read the root
    // directory information once this method successfully returns. Note that the
    // underlying medium is passed to the filesystem when it is created. Note
    // that the kernel guarantees that no operations will be issued to the
    // filesystem before start() has returned with EOK.
    // Override: Optional
    // Default Behavior: Returns EOK
    errno_t (*start)(void* _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize);

    // Invoked when a started (mounted) instance of this file system is stopped
    // (unmounted). A file system may return an error. Note however that this
    // error is purely informational and the file system implementation is
    // required to do everything it can to successfully stop. Errors returned by
    // this method are ignored and the file system manager will complete the
    // unmount operation in any case.
    // Override: Optional
    // Default Behavior: Returns EOK
    errno_t (*stop)(void* _Nonnull self);


    //
    // Filesystem Navigation
    //

    // Returns the root directory of the filesystem if the filesystem is currently
    // in mounted state. Returns EIO and NULL if the filesystem is not mounted.
    // Override: Advised
    // Default Behavior: Returns NULL and EIO
    errno_t (*acquireRootDirectory)(void* _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir);

    // Returns EOK and the node corresponding to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function has to support the special name ".." (parent of node) in addition
    // to "regular" filenames. If 'pDir' is the root directory of the filesystem
    // and 'pName' is ".." then 'pDir' should be returned again. If the
    // length of the path component is longer than what is supported by the
    // filesystem, then ENAMETOOLONG should be returned. The caller may pass a
    // pointer to a directory-entry-insertion-hint data structure. This function
    // may store information in this data structure to help speed up a follow up
    // CreateNode() call for a node with the name 'pName' in the directory
    // 'pDir'. You may pass NULL for 'pOutNode' which means that the function
    // will do the inode lookup and return a status that reflects the outcome
    // of the lookup, however the function will not return the looked up node.
    // You may use this mechanism to check whether a directory contains a node
    // with a given name without forcing the acquisition of the node itself.
    // Override: Advised
    // Default Behavior: Returns NULL and ENOENT (EIO for '..' lookups)
    errno_t (*acquireNodeForName)(void* _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);

    // Returns the name of the node with the id 'id' which a child of the
    // directory node 'pDir'. 'id' may be of any type. The name is returned in
    // the mutable path component 'pName'. The 'count' in the path component is
    // set to 0 on entry and should be set to the actual length of the name on
    // exit. The function is expected to return EOK if the directory contains
    // 'id' and ENOENT otherwise. If the name of 'id' as stored in the
    // filesystem is > the capacity of the path component, then ENAMETOOLONG
    // should be returned.
    // Override: Advised
    // Default Behavior: Returns EIO and sets 'pName' to an empty name
    errno_t (*getNameOfNode)(void* _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, uid_t uid, gid_t gid, MutablePathComponent* _Nonnull pName);


    //
    // General File/Directory Operations
    //

    // Creates a new inode with type 'type', user information 'user', permissions
    // 'permissions' and adds it to the directory 'pDir'. The new node will be
    // added to 'pDir' with the name 'pName'. Returns the newly acquired inode
    // on success and NULL otherwise.
    errno_t (*createNode)(void* _Nonnull self, FileType type, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode);

    // Unlink the node 'target' which is an immediate child of 'dir'. Both nodes
    // are guaranteed to be members of the same filesystem. 'target' is guaranteed
    // to exist and that it isn't a mountpoint and not the root node of the
    // filesystem.
    // This function must validate that that if 'target' is a directory, that the
    // directory is empty (contains nothing except "." and ".." entries).
    errno_t (*unlink)(void* _Nonnull self, InodeRef _Nonnull _Locked target, InodeRef _Nonnull _Locked dir);

    // Moves the node 'pSrcNode' from its current parent directory 'pSrcDir' to
    // the new parent directory 'pDstDir' and assigns it the name 'pName' in this
    // new directory.
    errno_t (*move)(void* _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint);

    // Changes the existing name of the node 'pSrcNode' which is an immediate
    // child of the directory 'pSrcDir' such that it will be 'pNewName'.
    errno_t (*rename)(void* _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid);


    //
    // Inode management
    //

    // Invoked when Filesystem_AcquireNodeWithId() needs to read the requested
    // inode off the disk. The override should read the inode data from the disk,
    // create an inode instance and fill it in with the data from the disk and
    // then return it. It should return a suitable error and NULL if the inode
    // data can not be read off the disk.
    // Override: Required
    // Default Behavior: Returns EIO
    errno_t (*onAcquireNode)(void* _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode);

    // Invoked when the inode is relinquished and it is marked as modified or
    // its link count is 0 and the filesystem is not read-only. The override
    // function should write the inode meta-data back to the disk. Note that
    // the override needs to check whether the link count is 0. A node with link
    // count 0 should be marked as deleted on disk and all disk blocks associated
    // with the node and its content should be freed. On the other hand, a node
    // with a link count > 0 should be kept alive and just updated on the disk.
    // Override: Required
    // Default Behavior: Returns EIO
    errno_t (*onWritebackNode)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode);

    // Invoked when the given inode should be freed. This function is called
    // after the last active reference to it has been relinquished and any
    // modified data has been written back to disk by onWritebackNode().
    // Override: Optional
    // Default behavior: unconditionally calls Inode_Destroy()
    void (*onRelinquishNode)(void* _Nonnull self, InodeRef _Nonnull pNode);


    //
    // Properties
    //

    // Returns true if the filesystem is read-only and false otherwise. A filesystem
    // may be read-only because it was mounted with a read-only parameter or
    // because the underlying disk is physically read-only.
    // Override: Optional
    // Default Behavior: Returns true
    bool (*isReadOnly)(void* _Nonnull self);
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

#define Filesystem_Start(__self, __pParams, __paramsSize) \
invoke_n(start, Filesystem, __self, __pParams, __paramsSize)

#define Filesystem_Stop(__self) \
invoke_0(stop, Filesystem, __self)


#define Filesystem_AcquireRootDirectory(__self, __pOutDir) \
invoke_n(acquireRootDirectory, Filesystem, __self, __pOutDir)

#define Filesystem_AcquireNodeForName(__self, __pDir, __pName, __uid, __gid, __pDirInsHint, __pOutNode) \
invoke_n(acquireNodeForName, Filesystem, __self, __pDir, __pName, __uid, __gid, __pDirInsHint, __pOutNode)

#define Filesystem_GetNameOfNode(__self, __pDir, __id, __uid, __gid, __pName) \
invoke_n(getNameOfNode, Filesystem, __self, __pDir, __id, __uid, __gid, __pName)


#define Filesystem_CreateNode(__self, __type, __pDir, __pName, __pDirInsertionHint, __uid, __gid, __permissions, __pOutNode) \
invoke_n(createNode, Filesystem, __self, __type, __pDir, __pName, __pDirInsertionHint, __uid, __gid, __permissions, __pOutNode)


#define Filesystem_Unlink(__self, __target, __dir) \
invoke_n(unlink, Filesystem, __self, __target, __dir)

#define Filesystem_Move(__self, __pSrcNode, __pSrcDir, __pDstDir, __pNewName, __uid, __gid, __pDirInstHint) \
invoke_n(move, Filesystem, __self, __pSrcNode, __pSrcDir, __pDstDir, __pNewName, __uid, __gid, __pDirInstHint)

#define Filesystem_Rename(__self, __pSrcNode, __pSrcDir, __pNewName, __uid, __gid) \
invoke_n(rename, Filesystem, __self, __pSrcNode, __pSrcDir, __pNewName, __uid, __gid)


// Acquires a new reference to the given node.
extern InodeRef _Nonnull _Locked Filesystem_ReacquireNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode);

// Relinquishes the given node back to the filesystem.
extern errno_t Filesystem_RelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nullable pNode);


//
// Methods for use by filesystem subclassers.
//

// Acquires the inode with the ID 'id'. This methods guarantees that there will
// always only be at most one inode instance in memory at any given time and
// that only one VP can access/modify the inode.
// Once you're done with the inode, you should relinquish it back to the filesystem.
// This method should be used by subclassers to acquire inodes in order to return
// them to a filesystem user.
// This method calls the filesystem method onAcquireNode() to read the requested
// inode off the disk if there is no inode instance in memory at the time this
// method is called.
// \param self the filesystem instance
// \param id the id of the inode to acquire
// \param pOutNode receives the acquired inode
extern errno_t Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode);

// Returns true if the filesystem can be unmounted which means that there are no
// acquired inodes outstanding that belong to this filesystem.
extern bool Filesystem_CanUnmount(FilesystemRef _Nonnull self);

#define Filesystem_OnAcquireNode(__self, __id, __pOutNode) \
invoke_n(onAcquireNode, Filesystem, __self, __id, __pOutNode)

#define Filesystem_OnWritebackNode(__self, __pNode) \
invoke_n(onWritebackNode, Filesystem, __self, __pNode)

#define Filesystem_OnRelinquishNode(__self, __pNode) \
invoke_n(onRelinquishNode, Filesystem, __self, __pNode)

#define Filesystem_IsReadOnly(__self) \
invoke_0(isReadOnly, Filesystem, __self)

#endif /* Filesystem_h */
