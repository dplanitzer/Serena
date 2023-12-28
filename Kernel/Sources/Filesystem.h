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
#include "Inode.h"

typedef struct _directory_entry_t   DirectoryEntry;


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


// Path component representing "."
extern const PathComponent kPathComponent_Self;

// Path component representing ".."
extern const PathComponent kPathComponent_Parent;

// Initializes a path component from a NUL-terminated string
extern PathComponent PathComponent_MakeFromCString(const Character* _Nonnull pCString);


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
// MARK: File
////////////////////////////////////////////////////////////////////////////////

OPEN_CLASS_WITH_REF(File, IOChannel,
    InodeRef _Nonnull   inode;
    FileOffset          offset;
);

typedef struct _FileMethodTable {
    IOChannelMethodTable    super;
} FileMethodTable;


// Creates a file object.
extern ErrorCode File_Create(FilesystemRef _Nonnull pFilesystem, UInt mode, InodeRef _Nonnull pNode, FileRef _Nullable * _Nonnull pOutFile);

// Creates a copy of the given file.
extern ErrorCode File_CreateCopy(FileRef _Nonnull pInFile, FileRef _Nullable * _Nonnull pOutFile);

// Returns the filesystem to which this file (I/O channel) connects
#define File_GetFilesystem(__self) \
    (FilesystemRef)IOChannel_GetResource((FileRef)__self)

// Returns the node that represents the physical file
#define File_GetInode(__self) \
    ((FileRef)__self)->inode

// Returns the offset at which the next read/write should start
#define File_GetOffset(__self) \
    ((FileRef)__self)->offset

// Increments the offset at which the next read/write should start
#define File_IncrementOffset(__self, __delta) \
    ((FileRef)__self)->offset += (FileOffset)(__delta)


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Directory
////////////////////////////////////////////////////////////////////////////////

// File positions/seeking and directories:
// The only allowed seeks are of the form seek(SEEK_SET) with an absolute position
// that was previously obtained from another seek or a value of 0 to rewind to the
// beginning of the directory listing. The seek position represents the index of
// the first directory entry that should be returned by the next read() operation.
// It is not a byte offset. This way it doesn't matter to the user of the read()
// and seek() call how exactly the contents of a directory is stored in the file
// system.  
OPEN_CLASS_WITH_REF(Directory, IOChannel,
    InodeRef _Nonnull   inode;
    FileOffset          offset;
);

typedef struct _DirectoryMethodTable {
    IOChannelMethodTable    super;
} DirectoryMethodTable;


// Creates a directory object.
extern ErrorCode Directory_Create(FilesystemRef _Nonnull pFilesystem, InodeRef _Nonnull pNode, DirectoryRef _Nullable * _Nonnull pOutDir);

// Creates a copy of the given directory descriptor.
extern ErrorCode Directory_CreateCopy(DirectoryRef _Nonnull pInDir, DirectoryRef _Nullable * _Nonnull pOutDir);

// Returns the filesystem to which this directory (I/O channel) connects
#define Directory_GetFilesystem(__self) \
    (FilesystemRef)IOChannel_GetResource((FileRef)__self)

// Returns the node that represents the physical directory
#define Directory_GetInode(__self) \
    ((DirectoryRef)__self)->inode

// Returns the index of the directory entry at which the next read should start
#define Directory_GetOffset(__self) \
    ((DirectoryRef)__self)->offset

// Sets the index of the directory entry that should be considered first on the next read
#define Directory_SetOffset(__self, __newOffset) \
    ((DirectoryRef)__self)->offset = (__newOffset)
    
// Increments the index of the directory entry at which the next read should start
#define Directory_IncrementOffset(__self, __delta) \
    ((DirectoryRef)__self)->offset += (__delta)


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

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
OPEN_CLASS(Filesystem, IOResource,
    FilesystemId        fsid;
    Lock                inodeManagementLock;
    PointerArray        inodesInUse;
);
typedef struct _FilesystemMethodTable {
    IOResourceMethodTable   super;

    //
    // Mounting/Unmounting
    //

    // Invoked when an instance of this file system is mounted. Note that the
    // kernel guarantees that no operations will be issued to the filesystem
    // before onMount() has returned with EOK.
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

    // Returns the root node of the filesystem if the filesystem is currently in
    // mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
    ErrorCode (*acquireRootNode)(void* _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode);

    // Returns EOK and the node that corresponds to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function has to support the special names "." (node itself) and ".."
    // (parent of node) in addition to "regular" filenames. If 'pParentNode' is
    // the root node of the filesystem and 'pComponent' is ".." then 'pParentNode'
    // should be returned. If the path component name is longer than what is
    // supported by the file system, ENAMETOOLONG should be returned.
    ErrorCode (*acquireNodeForName)(void* _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable _Locked * _Nonnull pOutNode);

    // Returns the name of the node with the id 'id' which a child of the
    // directory node 'pParentNode'. 'id' may be of any type. The name is
    // returned in the mutable path component 'pComponent'. 'count' in path
    // component is 0 on entry and should be set to the actual length of the
    // name on exit. The function is expected to return EOK if the parent node
    // contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
    // file system is > the capacity of the path component, then ERANGE should
    // be returned.
    ErrorCode (*getNameOfNode)(void* _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent);


    //
    // Get/Set Inode Attributes
    //

    // Returns a file info record for the given Inode. The Inode may be of any
    // file type.
    ErrorCode (*getFileInfo)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo);

    // Modifies one or more attributes stored in the file info record of the given
    // Inode. The Inode may be of any type.
    ErrorCode (*setFileInfo)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo);


    //
    // File Specific Operations
    //

    // Creates an empty file and returns the inode of that file. The behavior is
    // non-exclusive by default. Meaning the file is created if it does not 
    // exist and the file's inode is merrily acquired if it already exists. If
    // the mode is exclusive then the file is created if it doesn't exist and
    // an error is thrown if the file exists. Note that the file is not opened.
    // This must be done by calling the open() method.
    ErrorCode (*createFile)(void* _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions, InodeRef _Nullable _Locked * _Nonnull pOutNode);


    //
    // Directory Specific Operations
    //

    // Creates an empty directory as a child of the given directory node and with
    // the given name, user and file permissions. Returns EEXIST if a node with
    // the given name already exists.
    ErrorCode (*createDirectory)(void* _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions);

    // Opens the directory represented by the given node. Returns a directory
    // descriptor object which is teh I/O channel that allows you to read the
    // directory content.
    ErrorCode (*openDirectory)(void* _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir);

    // Reads the next set of directory entries. The first entry read is the one
    // at the current directory index stored in 'pDir'. This function guarantees
    // that it will only ever return complete directories entries. It will never
    // return a partial entry. Consequently the provided buffer must be big enough
    // to hold at least one directory entry. Note that this function is expected
    // to return "." for the entry at index #0 and ".." for the entry at index #1.
    ByteCount (*readDirectory)(void* _Nonnull self, DirectoryRef _Nonnull pDir, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);

    // Closes the given directory I/O channel.
    ErrorCode (*closeDirectory)(void* _Nonnull self, DirectoryRef _Nonnull pDir);


    //
    // General File/Directory Operations
    //

    // Verifies that the given node is accessible assuming the given access mode.
    ErrorCode (*checkAccess)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, Int mode);

    // Change the size of the file 'pNode' to 'length'. EINVAL is returned if
    // the new length is negative. No longer needed blocks are deallocated if
    // the new length is less than the old length and zero-fille blocks are
    // allocated and assigned to the file if the new length is longer than the
    // old length. Note that a filesystem implementation is free to defer the
    // actual allocation of the new blocks until an attempt is made to read or
    // write them.
    ErrorCode (*truncate)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FileOffset length);

    // Unlink the node 'pNode' which is an immediate child of 'pParentNode'.
    // Both nodes are guaranteed to be members of the same filesystem. 'pNode'
    // is guaranteed to exist and that it isn't a mountpoint and not the root
    // node of the filesystem.
    // This function must validate that that if 'pNode' is a directory, that the
    // directory is empty (contains nothing except "." and "..").
    ErrorCode (*unlink)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeRef _Nonnull _Locked pParentNode, User user);

    // Renames the node with name 'pName' and which is an immediate child of the
    // node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
    // the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
    ErrorCode (*rename)(void* _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pNewParentNode, User user);


    //
    // Required override points for subclassers
    //

    // Invoked when Filesystem_AllocateNode() is called. Subclassers should
    // override this method to allocate the on-disk representation of an inode
    // of the given type.
    ErrorCode (*onAllocateNodeOnDisk)(void* _Nonnull, InodeType type, void* _Nullable pContext, InodeId* _Nonnull pOutId);

    // Invoked when Filesystem_AcquireNodeWithId() needs to read the requested
    // inode off the disk. The override should read the inode data from the disk,
    // create and inode instance and fill it in with the data from the disk and
    // then return it. It should return a suitable error and NULL if the inode
    // data can not be read off the disk.
    // The 'pContext' argument is a filesystem implementation defined argument
    // which may be used to help in finding an inode on disk. Ie if the on-disk
    // data layout of a filesystem does not store inodes as independent objects
    // and instead stores them as directory entries inside a directory file then
    // this argument could be used as a pointer to the disk block(s) that hold
    // the directory content.
    ErrorCode (*onReadNodeFromDisk)(void* _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable * _Nonnull pOutNode);

    // Invoked when the inode is relinquished and it is marked as modified. The
    // filesystem override should write the inode meta-data back to the 
    // corresponding disk node.
    ErrorCode (*onWriteNodeToDisk)(void* _Nonnull self, InodeRef _Nonnull _Locked pNode);

    // Invoked when Filesystem_RelinquishNode() has determined that the inode is
    // no longer being referenced by any directory and that the on-disk
    // representation should be deleted from the disk and deallocated. This
    // operation is assumed to never fail.
    void (*onRemoveNodeFromDisk)(void* _Nonnull self, InodeId id);

} FilesystemMethodTable;


//
// Methods for use by filesystem users.
//

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


#define Filesystem_CreateFile(__self, __pName, __pParentNode, __user, __permissions, __pOutNode) \
Object_InvokeN(createFile, Filesystem, __self, __pName, __pParentNode, __user, __permissions, __pOutNode)


#define Filesystem_CreateDirectory(__self, __pName, __pParentNode, __user, __permissions) \
Object_InvokeN(createDirectory, Filesystem, __self, __pName, __pParentNode, __user, __permissions)

#define Filesystem_OpenDirectory(__self, __pDirNode, __user, __pOutDir) \
Object_InvokeN(openDirectory, Filesystem, __self, __pDirNode, __user, __pOutDir)

#define Filesystem_ReadDirectory(__self, __pDir, __pBuffer, __nBytesToRead) \
Object_InvokeN(readDirectory, Filesystem, __self, __pDir, __pBuffer, __nBytesToRead)

#define Filesystem_CloseDirectory(__self, __pDir) \
Object_InvokeN(closeDirectory, Filesystem, __self, __pDir)


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

// Allocates a new inode on disk and in-core. The allocation is protected
// by the same lock that is used to protect the acquisition, relinquishing,
// write-back and deletion of inodes. The returned inode id is not visible to
// any other thread of execution until it is explicitly shared with other code.
extern ErrorCode Filesystem_AllocateNode(FilesystemRef _Nonnull self, InodeType type, UserId uid, GroupId gid, FilePermissions permissions, void* _Nullable pContext, InodeRef _Nullable _Locked * _Nonnull pOutNode);

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
// \param pContext an optional context tp help the acquire method to find the inode
// \param pOutNode receives the acquired inode
extern ErrorCode Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable _Locked * _Nonnull pOutNode);

// Returns true if the filesystem can be safely unmounted which means that no
// inodes owned by the filesystem is currently in memory.
extern Bool Filesystem_CanSafelyUnmount(FilesystemRef _Nonnull self);

#define Filesystem_OnAllocateNodeOnDisk(__self, __type, __pContext, __pOutId) \
Object_InvokeN(onAllocateNodeOnDisk, Filesystem, __self, __type, __pContext, __pOutId)

#define Filesystem_OnReadNodeFromDisk(__self, __id, __pContext, __pOutNode) \
Object_InvokeN(onReadNodeFromDisk, Filesystem, __self, __id, __pContext, __pOutNode)

#define Filesystem_OnWriteNodeToDisk(__self, __pNode) \
Object_InvokeN(onWriteNodeToDisk, Filesystem, __self, __pNode)

#define Filesystem_OnRemoveNodeFromDisk(__self, __id) \
Object_InvokeN(onRemoveNodeFromDisk, Filesystem, __self, __id)

#endif /* Filesystem_h */
