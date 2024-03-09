//
//  PathResolver.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "PathResolver.h"
#include "FilesystemManager.h"

static void PathResolverResult_Init(PathResolverResult* pResult)
{
    pResult->inode = NULL;
    pResult->filesystem = NULL;
    pResult->lastPathComponent.name = NULL;
    pResult->lastPathComponent.count = 0;
}

void PathResolverResult_Deinit(PathResolverResult* pResult)
{
    if (pResult->inode) {
        Filesystem_RelinquishNode(pResult->filesystem, pResult->inode);
        pResult->inode = NULL;
    }
    Object_Release(pResult->filesystem);
    pResult->filesystem = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: InodeIterator
////////////////////////////////////////////////////////////////////////////////

// Tracks our current position in the global filesystem
typedef struct _InodeIterator {
    InodeRef _Nonnull _Locked   inode;
    FilesystemRef _Nonnull      filesystem;
} InodeIterator;

static void InodeIterator_Deinit(InodeIterator* _Nonnull pIterator);


static errno_t InodeIterator_Init(InodeIterator* _Nonnull pIterator, InodeRef _Nonnull pFirstNode)
{
    pIterator->filesystem = NULL;
    pIterator->inode = NULL;

    pIterator->filesystem = Inode_CopyFilesystem(pFirstNode);
    if (pIterator->filesystem == NULL) {
        return ENOENT;  // FS is no longer mounted
    }

    pIterator->inode = Filesystem_ReacquireNode(pIterator->filesystem, pFirstNode);
    return EOK;
}

static void InodeIterator_Deinit(InodeIterator* _Nonnull pIterator)
{
    if (pIterator->inode) {
        Filesystem_RelinquishNode(pIterator->filesystem, pIterator->inode);
        pIterator->inode = NULL;
    }
    Object_Release(pIterator->filesystem);
    pIterator->filesystem = NULL;
}

// Takes ownership of 'pNewNode' and expects that 'pNewNode' and the current node
// of the iterator are different. Updates the inode only (assumption is that the
// new and the old inode are located on the same filesystem).
static void InodeIterator_UpdateWithNodeOnly(InodeIterator* pIterator, InodeRef _Nonnull pNewNode)
{
    Filesystem_RelinquishNode(pIterator->filesystem, pIterator->inode);
    pIterator->inode = pNewNode;
}

// Takes ownership of 'pNewNode' and expects that 'pNewNode' and the current node
// of the iterator are different. Updates both the inode and the filesystem
// information.
static void InodeIterator_Update(InodeIterator* pIterator, InodeRef _Nonnull pNewNode)
{
    Filesystem_RelinquishNode(pIterator->filesystem, pIterator->inode);
    pIterator->inode = pNewNode;

    FilesystemRef pNewFileSys = Inode_CopyFilesystem(pNewNode);
    Object_Release(pIterator->filesystem);
    pIterator->filesystem = pNewFileSys;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: PathResolver
////////////////////////////////////////////////////////////////////////////////

static errno_t PathResolver_UpdateIteratorWalkingUp(PathResolverRef _Nonnull pResolver, User user, InodeIterator* _Nonnull pIter);


errno_t PathResolver_Init(PathResolverRef _Nonnull pResolver, InodeRef _Nonnull pRootDirectory, InodeRef _Nonnull pCurrentWorkingDirectory)
{
    decl_try_err();
    pResolver->rootDirectory = Inode_ReacquireUnlocked(pRootDirectory);
    pResolver->currentWorkingDirectory = Inode_ReacquireUnlocked(pCurrentWorkingDirectory);
    pResolver->pathComponent.name = pResolver->nameBuffer;
    pResolver->pathComponent.count = 0;
    
    return EOK;

catch:
    return err;
}

void PathResolver_Deinit(PathResolverRef _Nonnull pResolver)
{
    if (pResolver->rootDirectory) {
        Inode_Relinquish(pResolver->rootDirectory);
        pResolver->rootDirectory = NULL;
    }
    if (pResolver->currentWorkingDirectory) {
        Inode_Relinquish(pResolver->currentWorkingDirectory);
        pResolver->currentWorkingDirectory = NULL;
    }
}

static errno_t PathResolver_SetDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const char* _Nonnull pPath, InodeRef _Nullable * _Nonnull pDirToAssign)
{
    decl_try_err();
    PathResolverResult r;

    // Get the inode that represents the new directory
    try(PathResolver_AcquireNodeForPath(pResolver, kPathResolutionMode_TargetOnly, pPath, user, &r));


    // Make sure that it is actually a directory
    if (!Inode_IsDirectory(r.inode)) {
        throw(ENOTDIR);
    }


    // Make sure that we do have search permission on the last path component (directory)
    try(Filesystem_CheckAccess(r.filesystem, r.inode, user, kFilePermission_Execute));


    // Remember the new inode as our new directory
    if (*pDirToAssign) {
        Inode_Relinquish(*pDirToAssign);
        *pDirToAssign = NULL;
    }

    *pDirToAssign = r.inode;
    r.inode = NULL;

catch:
    PathResolverResult_Deinit(&r);
    return err;
}

errno_t PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const char* _Nonnull pPath)
{
    return PathResolver_SetDirectoryPath(pResolver, user, pPath, &pResolver->rootDirectory);
}

// Returns true if the given node represents the resolver's root directory.
bool PathResolver_IsRootDirectory(PathResolverRef _Nonnull pResolver, InodeRef _Nonnull _Locked pNode)
{
    return Inode_GetFilesystemId(pResolver->rootDirectory) == Inode_GetFilesystemId(pNode)
        && Inode_GetId(pResolver->rootDirectory) == Inode_GetId(pNode);
}

errno_t PathResolver_GetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, char* _Nonnull  pBuffer, size_t bufferSize)
{
    InodeIterator iter;
    decl_try_err();

    try(InodeIterator_Init(&iter, pResolver->currentWorkingDirectory));

    if (bufferSize < 1) {
        throw(EINVAL);
    }

    // We walk up the filesystem from 'pNode' to the global root of the filesystem
    // and we build the path right aligned in the user provided buffer. We'll move
    // the path such that it'll start at 'pBuffer' once we're done.
    MutablePathComponent pathComponent;
    char* p = &pBuffer[bufferSize - 1];
    *p = '\0';

    while (iter.inode != pResolver->rootDirectory) {
        const InodeId childInodeIdToLookup = Inode_GetId(iter.inode);

        try(PathResolver_UpdateIteratorWalkingUp(pResolver, user, &iter));

        pathComponent.name = pBuffer;
        pathComponent.count = 0;
        pathComponent.capacity = p - pBuffer;
        try(Filesystem_GetNameOfNode(iter.filesystem, iter.inode, childInodeIdToLookup, user, &pathComponent));

        p -= pathComponent.count;
        Bytes_CopyRange(p, pathComponent.name, pathComponent.count);

        if (p <= pBuffer) {
            throw(ERANGE);
        }

        *(--p) = '/';
    }

    if (*p == '\0') {
        if (p <= pBuffer) {
            throw(ERANGE);
        }
        *(--p) = '/';
    }
    
    Bytes_CopyRange(pBuffer, p, &pBuffer[bufferSize] - p);
    InodeIterator_Deinit(&iter);
    return EOK;

catch:
    InodeIterator_Deinit(&iter);
    pBuffer[0] = '\0';
    return err;
}

errno_t PathResolver_SetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const char* _Nonnull pPath)
{
    return PathResolver_SetDirectoryPath(pResolver, user, pPath, &pResolver->currentWorkingDirectory);
}

// Updates the given inode iterator with the parent node of the node to which the
// iterator points. Returns the iterator's inode itself if that inode is the path
// resolver's root directory. Returns a suitable error code and leaves the iterator
// unchanged if an error (eg access denied) occurs.
static errno_t PathResolver_UpdateIteratorWalkingUp(PathResolverRef _Nonnull pResolver, User user, InodeIterator* _Nonnull pIter)
{
    InodeRef _Locked pParentNode = NULL;
    InodeRef _Locked pMountingDir = NULL;
    InodeRef _Locked pParentOfMountingDir = NULL;
    FilesystemRef pMountingFilesystem = NULL;
    decl_try_err();

    // Nothing to do if the iterator points to our root node
    if (Inode_Equals(pIter->inode, pResolver->rootDirectory)) {
        return EOK;
    }


    try(Filesystem_AcquireNodeForName(pIter->filesystem, pIter->inode, &kPathComponent_Parent, user, &pParentNode));

    if (!Inode_Equals(pIter->inode, pParentNode)) {
        // We're moving to a parent node in the same file system
        InodeIterator_UpdateWithNodeOnly(pIter, pParentNode);
        return EOK;
    }

    // The pIter->inode is the root of a file system that is mounted somewhere
    // below the global file system root. We need to find the node in the parent
    // file system that is mounting pIter->inode and we the need to find the
    // parent of this inode. Note that such a parent always exists and that it
    // is necessarily in the same parent file system in which the mounting node
    // is (because you can not mount a file system on the root node of another
    // file system).
    try(FilesystemManager_CopyMountpointOfFilesystem(gFilesystemManager, pIter->filesystem, &pMountingDir, &pMountingFilesystem));
    try(Filesystem_AcquireNodeForName(pMountingFilesystem, pMountingDir, &kPathComponent_Parent, user, &pParentOfMountingDir));

    InodeIterator_Update(pIter, pParentOfMountingDir);
    Filesystem_RelinquishNode(pMountingFilesystem, pMountingDir);
    Object_Release(pMountingFilesystem);

    return EOK;

catch:
    if (pParentNode) {
        Inode_Relinquish(pParentNode);
    }
    if (pMountingDir) {
        Filesystem_RelinquishNode(pMountingFilesystem, pMountingDir);
    }
    Object_Release(pMountingFilesystem);
    return err;
}

// Updates the inode iterator with the inode that represents the given path
// component and returns EOK if that works out. Otherwise returns a suitable
// error and leaves the passed in iterator unchanged. This function handles the
// case that we want to walk down the filesystem tree or sideways (".").
static errno_t PathResolver_UpdateIteratorWalkingDown(PathResolverRef _Nonnull pResolver, User user, InodeIterator* _Nonnull pIter, const PathComponent* pComponent)
{
    InodeRef _Locked pChildNode = NULL;
    decl_try_err();

    // Ask the current filesystem for the inode that is named by the tuple
    // (parent-inode, path-component)
    try(Filesystem_AcquireNodeForName(pIter->filesystem, pIter->inode, pComponent, user, &pChildNode));


    // Note that if we do a lookup for ".", that we get back the same inode with
    // which we started but with an extra +1 ref count. We keep the iterator
    // intact and we drop the extra +1 ref in this case.
    if (pIter->inode == pChildNode) {
        Filesystem_RelinquishNode(pIter->filesystem, pChildNode);
        return EOK;
    }


    // Check whether the new inode is a mountpoint. If not then we just update
    // the iterator with the new node. If it is a mountpoint then we have to
    // switch to the mounted filesystem and its root node instead.
    FilesystemRef pMountedFileSys = FilesystemManager_CopyFilesystemMountedAtNode(gFilesystemManager, pChildNode);

    if (pMountedFileSys == NULL) {
        InodeIterator_UpdateWithNodeOnly(pIter, pChildNode);
    }
    else {
        InodeRef pMountedFileSysRootNode;

        try(Filesystem_AcquireRootNode(pMountedFileSys, &pMountedFileSysRootNode));
        Filesystem_RelinquishNode(pIter->filesystem, pChildNode);
        InodeIterator_Update(pIter, pMountedFileSysRootNode);
    }

catch:
    return err;
}

// Updates the inode iterator with the inode that represents the given path
// component and returns EOK if that works out. Otherwise returns a suitable
// error and leaves the passed in iterator unchanged.
static errno_t PathResolver_UpdateIterator(PathResolverRef _Nonnull pResolver, User user, InodeIterator* _Nonnull pIter, const PathComponent* pComponent)
{
    // The current directory better be an actual directory
    if (!Inode_IsDirectory(pIter->inode)) {
        return ENOTDIR;
    }


    // Walk up the filesystem tree if the path component is "..", sideways if
    // the path component is "." and down if it is any other name.
    if (pComponent->count == 2 && pComponent->name[0] == '.' && pComponent->name[1] == '.') {
        return PathResolver_UpdateIteratorWalkingUp(pResolver, user, pIter);
    }
    else {
        return PathResolver_UpdateIteratorWalkingDown(pResolver, user, pIter, pComponent);
    }
}

// Looks up the inode named by the given path. The path may be relative or absolute.
errno_t PathResolver_AcquireNodeForPath(PathResolverRef _Nonnull pResolver, PathResolutionMode mode, const char* _Nonnull pPath, User user, PathResolverResult* _Nonnull pResult)
{
    decl_try_err();
    InodeIterator iter;
    int pi = 0;

    PathResolverResult_Init(pResult);

    if (pPath[0] == '\0') {
        return ENOENT;
    }


    // Start with the root directory if the path starts with a '/' and the
    // current working directory otherwise
    InodeRef pStartingDir;
    if (pPath[0] == '/') {
        pStartingDir = pResolver->rootDirectory;
    } else {
        pStartingDir = pResolver->currentWorkingDirectory;
    }


    // Create our inode iterator
    try(InodeIterator_Init(&iter, pStartingDir));


    // Iterate through the path components, looking up the inode that corresponds
    // to the current path component. Stop once we hit the end of the path.
    while (true) {

        // Skip over (redundant) '/' character(s)
        while (pPath[pi] == '/') {
            if (pi >= kMaxPathLength) {
                throw(ENAMETOOLONG);
            }
            pi++;
        }
        

        // Pick up the next path component
        int ni = 0;
        while (pPath[pi] != '\0' && pPath[pi] != '/') {
            if (pi >= kMaxPathLength || ni >= kMaxPathComponentLength) {
                throw(ENAMETOOLONG);
            }
            pResolver->nameBuffer[ni++] = pPath[pi++];
        }


        // Treat a path that ends in a trailing '/' as if it would be "/."
        if (ni == 0) {
            pResolver->nameBuffer[0] = '.'; ni = 1;
        }
        pResolver->pathComponent.count = ni;


        // Check whether we're done if the resolution mode is ParentOnly
        if (mode == kPathResolutionMode_ParentOnly) {
            int si = pi;
            while(pPath[si] == '/') si++;

            if (pPath[si] == '\0') {
                // This is the last path component. The iterator is pointing at
                // the parent node.
                pResult->inode = iter.inode;
                pResult->filesystem = iter.filesystem;
                pResult->lastPathComponent.name = &pPath[pi - ni];
                pResult->lastPathComponent.count = ni;

                return EOK;
            }
        }


        // Ask the current namespace for the inode that is named by the tuple
        // (parent-inode, path-component)
        try(PathResolver_UpdateIterator(pResolver, user, &iter, &pResolver->pathComponent));


        // We're done if we've reached the end of the path. Otherwise continue
        // with the updated iterator
        if (pPath[pi] == '\0') {
            break;
        }
    }

    // Note that we move (ownership) of the target node & filesystem from the
    // iterator to the result
    pResult->inode = iter.inode;
    pResult->filesystem = iter.filesystem;
    pResult->lastPathComponent.name = &pPath[pi];
    pResult->lastPathComponent.count = 0;
    return EOK;

catch:
    InodeIterator_Deinit(&iter);
    return err;
}
