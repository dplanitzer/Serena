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
    Int                 offset;
);

typedef struct _DirectoryMethodTable {
    IOChannelMethodTable    super;
} DirectoryMethodTable;


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

    // Returns a strong reference to the root directory node of the filesystem.
    InodeRef _Nonnull (*copyRootNode)(void* _Nonnull self);

    // Returns EOK and the node that corresponds to the tuple (parent-node, name),
    // if that node exists. Otherwise returns ENOENT and NULL.  Note that this
    // function has the support the special names "." (node itself) and ".."
    // (parent of node) in addition to "regular" filenames. If the path component
    // name is longer than what is supported by the file system, ENAMETOOLONG
    // should be returned.
    ErrorCode (*copyNodeForName)(void* _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable * _Nonnull pOutNode);

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

    // Returns a file info record for the given Inode. The Inode may be of any
    // file type.
    ErrorCode (*getFileInfo)(void* _Nonnull self, InodeRef _Nonnull pNode, FileInfo* _Nonnull pOutInfo);

    // Modifies one or more attributes stored in the file info record of the given
    // Inode. The Inode may be of any type.
    ErrorCode (*setFileInfo)(void* _Nonnull self, InodeRef _Nonnull pNode, User user, MutableFileInfo* _Nonnull pInfo);


    //
    // Directory Operations
    //

    // Creates an empty directory as a child of the given directory node and with
    // the given name, user and file permissions
    ErrorCode (*createDirectory)(void* _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pName, User user, FilePermissions permissions);

    // Opens the directory represented by the given node. Returns a directory
    // descriptor object which is teh I/O channel that allows you to read the
    // directory content.
    ErrorCode (*openDirectory)(void* _Nonnull self, InodeRef _Nonnull pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir);

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
    ErrorCode (*checkAccess)(void* _Nonnull self, InodeRef _Nonnull pNode, User user, Int mode);

    // Unlink the node 'pNode' which is an immediate child of the node 'pParentNode'.
    // The parent node is guaranteed to be a node owned by the filesystem.
    ErrorCode (*unlink)(void* _Nonnull self, InodeRef _Nonnull pNode, InodeRef _Nonnull pParentNode, User user);

    // Renames the node 'pNode' which is an immediate child of the node 'pParentNode'.
    // such that it becomes a child of 'pNewParentNode' with the name 'pNewName'.
    // All nodes are guaranteed to be owned by the filesystem.
    ErrorCode (*rename)(void* _Nonnull self, InodeRef _Nonnull pNode, InodeRef _Nonnull pParentNode, const PathComponent* pNewName, InodeRef _Nonnull pNewParentNode, User user);

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

#define Filesystem_CopyNodeForName(__self, __pParentNode, __pComponent, __user, __pOutNode) \
Object_InvokeN(copyNodeForName, Filesystem, __self, __pParentNode, __pComponent, __user, __pOutNode)

#define Filesystem_GetNameOfNode(__self, __pParentNode, __pNode, __user, __pComponent) \
Object_InvokeN(getNameOfNode, Filesystem, __self, __pParentNode, __pNode, __user, __pComponent)


#define Filesystem_GetFileInfo(__self, __pNode, __pOutInfo) \
Object_InvokeN(getFileInfo, Filesystem, __self, __pNode, __pOutInfo)

#define Filesystem_SetFileInfo(__self, __pNode, __user, __pInfo) \
Object_InvokeN(setFileInfo, Filesystem, __self, __pNode, __user, __pInfo)


#define Filesystem_CreateDirectory(__self, __pParentNode, __pName, __user, __permissions) \
Object_InvokeN(createDirectory, Filesystem, __self, __pParentNode, __pName, __user, __permissions)

#define Filesystem_OpenDirectory(__self, __pDirNode, __user, __pOutDir) \
Object_InvokeN(openDirectory, Filesystem, __self, __pDirNode, __user, __pOutDir)

#define Filesystem_ReadDirectory(__self, __pDir, __pBuffer, __nBytesToRead) \
Object_InvokeN(readDirectory, Filesystem, __self, __pDir, __pBuffer, __nBytesToRead)

#define Filesystem_CloseDirectory(__self, __pDir) \
Object_InvokeN(closeDirectory, Filesystem, __self, __pDir)


#define Filesystem_CheckAccess(__self, __pNode, __user, __mode) \
Object_InvokeN(checkAccess, Filesystem, __self, __pNode, __user, __mode)

#define Filesystem_Unlink(__self, __pNode, __pParentNode, __user) \
Object_InvokeN(unlink, Filesystem, __self, __pNode, __pParentNode, __user)

#define Filesystem_Rename(__self, __pNode, __pParentNode, __pNewName, __pNewParentNode, __user) \
Object_InvokeN(rename, Filesystem, __self, __pNode, __pParentNode, __pNewName, __pNewParentNode, __user)

#endif /* Filesystem_h */
