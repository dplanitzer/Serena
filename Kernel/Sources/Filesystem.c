//
//  Filesystem.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Filesystem.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Path Component
////////////////////////////////////////////////////////////////////////////////

// Initializes a path component from a NUL-terminated string
PathComponent PathComponent_MakeFromCString(const Character* _Nonnull pCString)
{
    PathComponent pc;

    pc.name = pCString;
    pc.count = String_Length(pCString);
    return pc;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// Returns the next available FSID.
static FilesystemId Filesystem_GetNextAvailableId(void)
{
    // XXX want to:
    // XXX handle overflow (wrap around)
    // XXX make sure the generated id isn't actually in use by someone else
    static volatile AtomicInt gNextAvailableId = 0;
    return (FilesystemId) AtomicInt_Increment(&gNextAvailableId);
}

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
ErrorCode Filesystem_Create(ClassRef pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    FilesystemRef pFileSys;

    try(_Object_Create(pClass, 0, (ObjectRef*)&pFileSys));
    pFileSys->fsid = Filesystem_GetNextAvailableId();
    *pOutFileSys = pFileSys;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

// Invoked when an instance of this file system is mounted.
ErrorCode Filesystem_onMount(void* _Nonnull self, const Byte* _Nonnull pParams, ByteCount paramsSize)
{
    return EOK;
}

// Invoked when a mounted instance of this file system is unmounted. A file
// system may return an error. Note however that this error is purely advisory
// and the file system implementation is required to do everything it can to
// successfully unmount. Unmount errors are ignored and the file system manager
// will complete the unmount in any case.
ErrorCode Filesystem_onUnmount(void* _Nonnull self)
{
    return EOK;
}


// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
InodeRef _Nonnull Filesystem_copyRootNode(FilesystemRef _Nonnull self)
{
    abort();
    // NOT REACHED
    return NULL;
}

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
ErrorCode Filesystem_copyParentOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function will always only be called with proper node names. Eg never with
// "." nor "..".
ErrorCode Filesystem_copyNodeForName(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

ErrorCode Filesystem_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, User user, MutablePathComponent* _Nonnull pComponent)
{
    pComponent->count = 0;
    return ENOENT;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
ErrorCode Filesystem_getFileInfo(void* _Nonnull self, InodeRef _Nonnull pNode, FileInfo* _Nonnull pOutInfo)
{
    return ENOENT;
}

// If the node is a directory and another file system is mounted at this directory,
// then this function returns the filesystem ID of the mounted directory; otherwise
// 0 is returned.
FilesystemId Filesystem_getFilesystemMountedOnNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    return 0;
}

// Marks the given directory node as a mount point at which the filesystem
// with the given filesystem ID is mounted. Converts the node back into a
// regular directory node if the give filesystem ID is 0.
void Filesystem_setFilesystemMountedOnNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, FilesystemId fsid)
{
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions
ErrorCode Filesystem_createDirectory(void* _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pName, User user, FilePermissions permissions)
{
    return EACCESS;
}

CLASS_METHODS(Filesystem, IOResource,
METHOD_IMPL(onMount, Filesystem)
METHOD_IMPL(onUnmount, Filesystem)
METHOD_IMPL(copyRootNode, Filesystem)
METHOD_IMPL(copyParentOfNode, Filesystem)
METHOD_IMPL(copyNodeForName, Filesystem)
METHOD_IMPL(getNameOfNode, Filesystem)
METHOD_IMPL(getFileInfo, Filesystem)
METHOD_IMPL(getFilesystemMountedOnNode, Filesystem)
METHOD_IMPL(setFilesystemMountedOnNode, Filesystem)
METHOD_IMPL(createDirectory, Filesystem)
);
