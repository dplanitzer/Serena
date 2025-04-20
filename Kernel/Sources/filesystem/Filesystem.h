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
#ifndef __DISKIMAGE__
#include <Catalog.h>
#else
typedef uint32_t CatalogId;
#define kCatalogId_None 0
#endif
#include <System/Filesystem.h>
#include <System/User.h>


// Can be used by filesystem subclasses to temporarily store a hint where a new
// directory entry may be inserted in a directory structure. This hint is returned
// by the AcquireNodeForName() function and it is passed to the CreateNode()
// function. This structure is able to store 32 bytes and guarantees that the
// data is aligned suitably for 64bit words.
typedef struct DirectoryEntryInsertionHint {
    uint64_t    data[4];
} DirectoryEntryInsertionHint;


// Filesystem properties returned by the onStart() override
typedef struct FSProperties {
    ino_t   rootDirectoryId;
    bool    isReadOnly;
} FSProperties;


enum {
    kFilesystemState_Idle = 0,
    kFilesystemState_Active,
    kFilesystemState_Stopped
};


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
// the data on this medium. A medium can be anything as far as the abstract
// Filesystem base class is concerned: it may be a physical disk, a tape or maybe
// some kind of object that only exists in memory and does not even persist across
// reboot.
// The ContainerFilesystem subclass is a specialization of Filesystem which builds
// on top of a FSContainer. A FSContainer represents a logical disk which may map
// 1:1 to a single physical disk or an array of physical disks. Concrete filesystem
// implementations which are meant to implement a traditional disk-based filesystem
// should derive from ContainerFilesystem instead of Filesystem.
//
//
// Filesystem and inode lifetimes:
//
// The lifetime of a filesystem instance is always >= the lifetime of all
// acquired inodes. This is guaranteed by ensuring that a filesystem can not be
// stopped and destroyed as long as there is at least one acquired inode
// outstanding. Thus it is sufficient to either hold a strong reference to a
// filesystem object (use Object_Retain() to get it) or a strong reference to
// an inode from the filesystem in question (use Filesystem_AcquireNode() to get
// one) to ensure that the filesystem stays alive.
//
//
// Starting/stopping a filesystem:
//
// A filesystem must be started before it can be used and the root inode can be
// acquired. Conversely all acquired inodes must have been relinquished before
// the filesystem can be stopped and destroyed. However a filesystem may be
// force-stopped which means that the filesystem is removed from the file
// hierarchy (and thus is no longer accessible by any process) and the actual
// stop and destruction action is deferred until the last acquired inode is
// relinquished. Note that a particular filesystem instance can be started at
// most once in its lifetime.
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
// Reacquiring an inode is also atomic.
//
// Filesystem Start, Stop and Root Node Acquisition:
//
// Starting, stopping a filesystem and acquiring its root node (and any other
// node for that matter) are protected by the same inode management lock and
// are atomic with respect to each other. A filesystem must be started and not
// stopped in order to be able to acquire its root node or any other node. This
// mechanism is enforced by the Filesystem class and subclasses do not need to
// implement anything special to make this semantic work. Subclasses simply
// override the onStart() and onStop() methods to implement the filesystem
// specific portions of starting and stopping the filesystem.
//
// Remember that a filesystem can not be stopped and neither deallocated as
// long as there is at least one acquired inode outstanding. Because of this and
// the fact that all filesystem operations expect at least one inode as input,
// non of the filesystem operation functions need to be protected with a
// filesystem specific lock.
// The inode that is passed to an operation acts in a sense as a lock and its
// existence guarantees that the filesystem can not be stopped and deallocated
// while the operation is executing.
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
    ino_t               rootDirectoryId;
    int8_t              state;
    bool                isReadOnly;
    int8_t              reserved[2];
    CatalogId           catalogId;
    int                 useCount;   // Protected by inLock
    int                 openChannelsCount;  // Protected by inLock
);
open_class_funcs(Filesystem, Object,

    //
    // FS management
    //

    // Invoked when an instance of this file system is started. Overrides of this
    // method should read the root information off the underlying medium and
    // prepare the filesystem for use. Ie it must be possible to read the root
    // directory information once this method successfully returns. Note that the
    // underlying medium is passed to the filesystem when it is created. Note
    // that the kernel guarantees that no operations will be issued to the
    // filesystem before start() has returned with EOK.
    // Override: Required
    // Default Behavior: Returns EOK
    errno_t (*onStart)(void* _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize, FSProperties* _Nonnull pOutProps);

    // Invoked when a started instance of this file system is stopped. A file
    // system may return an error. Note however that this error is purely
    // informational and the file system implementation is required to do
    // everything it can to successfully stop. Errors returned by this method
    // are ignored and the file system manager will complete the stop operation
    // in any case.
    // A stopped filesystem may not be restarted and no more inodes can be
    // acquired.
    // Note that overrides of this method are expected to sync dirty blocks that
    // belong to this filesystem to the disk before this function returns.
    // Override: Optional
    // Default Behavior: Returns EOK
    errno_t (*onStop)(void* _Nonnull self);


    // Invoked as the result of calling Filesystem_Open(). A filesystem subclass
    // may override this method to create a filesystem I/O channel object. The
    // default implementation creates a FSChannel instance which supports
    // ioctl operations.
    // Override: Optional
    // Default Behavior: Creates a FSChannel instance
    errno_t (*open)(void* _Nonnull _Locked self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);

    // Invoked as the result of calling Filesystem_Close().
    // Override: Optional
    // Default Behavior: Does nothing and returns EOK
    errno_t (*close)(void* _Nonnull _Locked self, IOChannelRef _Nonnull pChannel);


    // Returns general information about the filesystem.
    // Override: Optional
    // Default Behavior: Returns ENOTIOCTLCMD
    errno_t (*getInfo)(void* _Nonnull self, FSInfo* _Nonnull pOutInfo);

    // Returns the canonical name of the disk on which this filesystem resides.
    // Note that a filesystem may actually not be backed by a disk. An empty
    // string is returned in this case or any other case in which the disk name
    // can not be established.
    // Override: Optional
    // Default Behavior: Returns an empty string
    errno_t (*getDiskName)(void* _Nonnull self, size_t bufSize, char* _Nonnull buf);

    // Returns the filesystem's label. A label is a string that is assigned to a
    // filesystem when it is formatted. However, it may be changed at a later
    // time by calling setLabel().
    // Override: Optional
    // Default Behavior: Returns ENOTSUP
    errno_t (*getLabel)(void* _Nonnull self, size_t bufSize, char* _Nonnull buf);

    // Sets the filesystem's label.
    // Override: Optional
    // Default Behavior: Returns ENOTSUP
    errno_t (*setLabel)(void* _Nonnull self, const char* _Nonnull buf);


    // Invoked as the result of calling Filesystem_Ioctl(). A filesystem subclass
    // should override this method to implement support for the ioctl() system
    // call.
    // Override: Optional
    // Default Behavior: Returns ENOTIOCTLCMD
    errno_t (*ioctl)(void* _Nonnull self, int cmd, va_list ap);


    //
    // Filesystem Navigation
    //

    // Returns the parent node (directory) of the node 'pNode'. 'pNode' may be
    // a directory, file or other type of node. However, only retrieval of a
    // parent of a directory is guaranteed to be supported by a filesystem. Other
    // types of nodes may or may not be supported. ENOTSUP is returned in this
    // case. Otherwise EOK and the parent node is returned. The node is returned
    // unlocked. If 'pNode' is the root node of the filesystem then 'pOutParent'
    // is set to 'pNode' and 'pNode' is reacquired.
    // Override: Advised
    // Default Behavior: Calls Inode.getParentId()
    errno_t (*acquireParentNode)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeRef _Nullable * _Nonnull pOutParent);

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
    errno_t (*acquireNodeForName)(void* _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode);

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
    errno_t (*getNameOfNode)(void* _Nonnull self, InodeRef _Nonnull _Locked pDir, ino_t id, MutablePathComponent* _Nonnull pName);

    
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
);


//
// Methods for use by filesystem users.
//

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
extern errno_t Filesystem_Create(Class* pClass, FilesystemRef _Nullable * _Nonnull pOutSelf);

// Returns the filesystem ID of the given filesystem.
#define Filesystem_GetId(__fs) \
    ((FilesystemRef)(__fs))->fsid

extern errno_t Filesystem_Start(FilesystemRef _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize);
extern errno_t Filesystem_Stop(FilesystemRef _Nonnull self);


extern void Filesystem_BeginUse(FilesystemRef _Nonnull self);
extern void Filesystem_EndUse(FilesystemRef _Nonnull self);


extern errno_t Filesystem_Publish(FilesystemRef _Nonnull self);
extern errno_t Filesystem_Unpublish(FilesystemRef _Nonnull self);


// Opens an I/O channel to the filesystem with the mode 'mode'. EOK and the channel
// is returned in 'pOutChannel' on success and a suitable error code is returned
// otherwise.
#define Filesystem_Open(__self, __mode, __arg, __pOutChannel) \
invoke_n(open, Filesystem, __self, __mode, __arg, __pOutChannel)

// Closes the given filesystem channel.
#define Filesystem_Close(__self, __ch) \
invoke_n(close, Filesystem, __self, __ch)


#define Filesystem_GetInfo(__self, __pOutInfo) \
invoke_n(getInfo, Filesystem, __self, __pOutInfo)

#define Filesystem_GetDiskName(__self, __bufSize, __buf) \
invoke_n(getDiskName, Filesystem, __self, __bufSize, __buf)

#define Filesystem_GetLabel(__self, __bufSize, __buf) \
invoke_n(getLabel, Filesystem, __self, __bufSize, __buf)

#define Filesystem_SetLabel(__self, __buf) \
invoke_n(setLabel, Filesystem, __self, __buf)


extern errno_t Filesystem_Ioctl(FilesystemRef _Nonnull self, int cmd, ...);

#define Filesystem_vIoctl(__self, __cmd, __ap) \
invoke_n(ioctl, Filesystem, __self, __cmd, __ap)


extern errno_t Filesystem_AcquireRootDirectory(FilesystemRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir);

#define Filesystem_AcquireParentNode(__self, __pNode, __pOutParent) \
invoke_n(acquireParentNode, Filesystem, __self, __pNode, __pOutParent)

#define Filesystem_AcquireNodeForName(__self, __pDir, __pName, __pDirInsHint, __pOutNode) \
invoke_n(acquireNodeForName, Filesystem, __self, __pDir, __pName, __pDirInsHint, __pOutNode)

#define Filesystem_GetNameOfNode(__self, __pDir, __id, __pName) \
invoke_n(getNameOfNode, Filesystem, __self, __pDir, __id, __pName)


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

#define Filesystem_IsReadOnly(__self) \
((FilesystemRef)__self)->isReadOnly


//
// Methods for use by filesystem subclassers.
//

#define Filesystem_OnStart(__self, __pParams, __paramsSize, __fsProps) \
invoke_n(onStart, Filesystem, __self, __pParams, __paramsSize, __fsProps)

#define Filesystem_OnStop(__self) \
invoke_0(onStop, Filesystem, __self)

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

#define Filesystem_OnAcquireNode(__self, __id, __pOutNode) \
invoke_n(onAcquireNode, Filesystem, __self, __id, __pOutNode)

#define Filesystem_OnWritebackNode(__self, __pNode) \
invoke_n(onWritebackNode, Filesystem, __self, __pNode)

#define Filesystem_OnRelinquishNode(__self, __pNode) \
invoke_n(onRelinquishNode, Filesystem, __self, __pNode)

#endif /* Filesystem_h */
