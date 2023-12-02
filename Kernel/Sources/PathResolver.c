//
//  PathResolver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "PathResolver.h"
#include "FilesystemManager.h"

static void PathResolverResult_Init(PathResolverResult* pResult, const Character* _Nonnull pPath)
{
    pResult->inode = NULL;
    pResult->fileSystem = NULL;
    pResult->parentInode = NULL;
    pResult->parentFileSystem = NULL;
    pResult->pathSuffix = pPath;
}

void PathResolverResult_Deinit(PathResolverResult* pResult)
{
    Object_Release(pResult->inode);
    pResult->inode = NULL;
    Object_Release(pResult->fileSystem);
    pResult->fileSystem = NULL;

    Object_Release(pResult->parentInode);
    pResult->parentInode = NULL;
    Object_Release(pResult->parentFileSystem);
    pResult->parentFileSystem = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: InodeIterator
////////////////////////////////////////////////////////////////////////////////

// Tracks our current position in the global filesystem
typedef struct _InodeIterator {
    InodeRef _Nonnull       inode;
    FilesystemRef _Nonnull  fileSystem;
    FilesystemId            fsid;
} InodeIterator;

static void InodeIterator_Deinit(InodeIterator* pIterator);


static ErrorCode InodeIterator_Init(InodeIterator* pIterator, InodeRef pFirstNode)
{
    pIterator->inode = Object_RetainAs(pFirstNode, Inode);
    pIterator->fsid = Inode_GetFilesystemId(pFirstNode);
    pIterator->fileSystem = FilesystemManager_CopyFilesystemForId(gFilesystemManager, pIterator->fsid);

    if (pIterator->fileSystem == NULL) {
        InodeIterator_Deinit(pIterator);
        return ENOENT;
    } else {
        return EOK;
    }
}

static void InodeIterator_Deinit(InodeIterator* pIterator)
{
    Object_Release(pIterator->inode);
    pIterator->inode = NULL;
    Object_Release(pIterator->fileSystem);
    pIterator->fileSystem = NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: PathResolver
////////////////////////////////////////////////////////////////////////////////

static ErrorCode PathResolver_UpdateIteratorWithParentNode(PathResolverRef _Nonnull pResolver, User user, InodeIterator* _Nonnull pIter);


ErrorCode PathResolver_Init(PathResolverRef _Nonnull pResolver, InodeRef _Nonnull pRootDirectory, InodeRef _Nonnull pCurrentWorkingDirectory)
{
    decl_try_err();
    pResolver->rootDirectory = Object_RetainAs(pRootDirectory, Inode);
    pResolver->currentWorkingDirectory = Object_RetainAs(pCurrentWorkingDirectory, Inode);
    pResolver->pathComponent.name = pResolver->nameBuffer;
    pResolver->pathComponent.count = 0;
    
    return EOK;

catch:
    return err;
}

void PathResolver_Deinit(PathResolverRef _Nonnull pResolver)
{
    Object_Release(pResolver->rootDirectory);
    pResolver->rootDirectory = NULL;
    Object_Release(pResolver->currentWorkingDirectory);
    pResolver->currentWorkingDirectory = NULL;
}

static ErrorCode PathResolver_SetDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* _Nonnull pPath, InodeRef _Nullable * _Nonnull pDirToAssign)
{
    decl_try_err();
    PathResolverResult r;

    // Get the inode that represents the new directory
    try(PathResolver_CopyNodeForPath(pResolver, kPathResolutionMode_TargetOnly, pPath, user, &r));


    // Make sure that it is actually a directory
    if (!Inode_IsDirectory(r.inode)) {
        throw(ENOTDIR);
    }


    // Make sure that we do have search permission on the last path component (directory)
    try(Filesystem_CheckAccess(r.fileSystem, r.inode, user, kFilePermission_Execute));


    // Remember the new inode as our new directory
    Object_Assign(pDirToAssign, r.inode);

catch:
    PathResolverResult_Deinit(&r);
    return err;
}

ErrorCode PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* pPath)
{
    return PathResolver_SetDirectoryPath(pResolver, user, pPath, &pResolver->rootDirectory);
}

ErrorCode PathResolver_GetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, Character* pBuffer, ByteCount bufferSize)
{
    InodeIterator iter;
    InodeRef pCurNode = NULL;
    decl_try_err();

    try(InodeIterator_Init(&iter, pResolver->currentWorkingDirectory));

    if (bufferSize <= 0) {
        throw(EINVAL);
    }

    // We walk up the filesystem from 'pNode' to the global root of the filesystem
    // and we build the path right aligned in the user provided buffer. We'll move
    // the path such that it'll start at 'pBuffer' once we're done.
    MutablePathComponent pathComponent;
    Character* p = &pBuffer[bufferSize - 1];
    *p = '\0';

    while (iter.inode != pResolver->rootDirectory) {
        Object_Release(pCurNode);
        pCurNode = Object_RetainAs(iter.inode, Inode);

        try(PathResolver_UpdateIteratorWithParentNode(pResolver, user, &iter));

        pathComponent.name = pBuffer;
        pathComponent.count = 0;
        pathComponent.capacity = p - pBuffer;
        try(Filesystem_GetNameOfNode(iter.fileSystem, iter.inode, pCurNode, user, &pathComponent));

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
    Object_Release(pCurNode);
    InodeIterator_Deinit(&iter);

    return EOK;

catch:
    Object_Release(pCurNode);
    InodeIterator_Deinit(&iter);
    pBuffer[0] = '\0';

    return err;
}

ErrorCode PathResolver_SetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* _Nonnull pPath)
{
    return PathResolver_SetDirectoryPath(pResolver, user, pPath, &pResolver->currentWorkingDirectory);
}

// Updates the given inode iterator with the parent node of the node to which the
// iterator points. Returns the iterator's inode itself if that inode is the path
// resolver's root directory. Returns a suitable error code and leaves the iterator
// unchanged if an error (eg access denied) occurs.
static ErrorCode PathResolver_UpdateIteratorWithParentNode(PathResolverRef _Nonnull pResolver, User user, InodeIterator* _Nonnull pIter)
{
    // Nothing to do if the iterator points to our root node
    if (Inode_Equals(pIter->inode, pResolver->rootDirectory)) {
        return EOK;
    }


    InodeRef parentNode;
    ErrorCode err = Filesystem_CopyParentOfNode(pIter->fileSystem, pIter->inode, user, &parentNode);

    if (err == EOK) {
        // We're moving to a parent node in the same file system
        Object_Release(pIter->inode);
        pIter->inode = parentNode;
        return EOK;
    }
    
    if (err != ENOENT) {
        // Some error (eg insufficient permissions). Just return
        return err;
    }

    // The pIter->inode is the root of a file system that is mounted somewhere
    // below the global file system root. We need to find the node in the parent
    // file system that is mounting pIter->inode and we the need to find the
    // parent of this inode. Note that such a parent always exists and that it
    // is necessarily in the same parent file system in which the mounting node
    // is (because you can not mount a file system on the root node of another
    // file system).
    InodeRef pMountingDir;
    FilesystemRef pMountingFilesystem;

    err = FilesystemManager_CopyNodeAndFilesystemMountingFilesystemId(gFilesystemManager, pIter->fsid, &pMountingDir, &pMountingFilesystem);
    if (err != EOK) {
        Object_Release(parentNode);
        return err;
    }

    err = Filesystem_CopyParentOfNode(pMountingFilesystem, pMountingDir, user, &parentNode);
    if (err != EOK) {
        Object_Release(parentNode);
        Object_Release(pMountingDir);
        Object_Release(pMountingFilesystem);
        return err;
    }

    Object_AssignMovingOwnership(&pIter->inode, parentNode);
    Object_AssignMovingOwnership(&pIter->fileSystem, pMountingFilesystem);
    pIter->fsid = Filesystem_GetId(pMountingFilesystem);

    return EOK;
}

// Updates the inode iterator with the inode that represents the given path
// component and returns EOK if that works out. Otherwise returns a suitable
// error and leaves the passed in iterator unchanged.
static ErrorCode PathResolver_UpdateIterator(PathResolverRef _Nonnull pResolver, User user, InodeIterator* _Nonnull pIter, const PathComponent* pComponent)
{
    // The current directory better be an actual directory
    if (!Inode_IsDirectory(pIter->inode)) {
        return ENOTDIR;
    }


    // Handle ".."
    if (pComponent->count == 2 && pComponent->name[0] == '.' && pComponent->name[1] == '.') {
        return PathResolver_UpdateIteratorWithParentNode(pResolver, user, pIter);
    }


    // Ask the current filesystem for the inode that is named by the tuple
    // (parent-inode, path-component)
    InodeRef pChildNode;
    const ErrorCode err = Filesystem_CopyNodeForName(pIter->fileSystem, pIter->inode, pComponent, user, &pChildNode);
    if (err != EOK) {
        return err;
    }


    // Just return the node we looked up without changing the filesystem if the
    // node isn't a mount point
    const FilesystemId mountedFsId = Filesystem_GetFilesystemMountedOnNode(pIter->fileSystem, pChildNode);
    if (mountedFsId == 0) {
        Object_Assign(&pIter->inode, pChildNode);
        return EOK;
    }


    // The node is a mount point. Figure out what the mounted filesystem is
    // and return the root node of this filesystem
    FilesystemRef mountedFilesystem = FilesystemManager_CopyFilesystemForId(gFilesystemManager, mountedFsId);
    if (mountedFilesystem == NULL) {
        return ENOENT;
    }

    Object_Release(pChildNode);
    pIter->fsid = Filesystem_GetId(mountedFilesystem);
    Object_AssignMovingOwnership(&pIter->fileSystem, mountedFilesystem);
    Object_AssignMovingOwnership(&pIter->inode, Filesystem_CopyRootNode(mountedFilesystem));

    return EOK;
}

// Looks up the inode named by the given path. The path may be relative or absolute.
// If it is relative then the resolution starts with the current working directory.
// If it is absolute then the resolution starts with the root directory. The path
// may contain the well-known name '.' which stands for 'this directory' and '..'
// which stands for 'the parent directory'. Note that this function does not allow
// you to leave the subtree rotted by the root directory. Any attempt to go to a
// parent of the root directory will send you back to the root directory.
// Note that the caller of this function has to eventually call
// PathResolverResult_Deinit() on the returned result no matter whether this
// function has returned with EOK or some error.
ErrorCode PathResolver_CopyNodeForPath(PathResolverRef _Nonnull pResolver, PathResolutionMode mode, const Character* _Nonnull pPath, User user, PathResolverResult* _Nonnull pResult)
{
    decl_try_err();
    InodeIterator iter;
    Int pi = 0;

    PathResolverResult_Init(pResult, pPath);

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
        Int ni = 0, pathSuffixStartIdx = pi;
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


        if (mode == kPathResolutionMode_TargetAndParent) {
            Object_Assign(&pResult->parentInode, iter.inode);
            Object_Assign(&pResult->parentFileSystem, iter.fileSystem);
        }


        // Ask the current namespace for the inode that is named by the tuple
        // (parent-inode, path-component)
        err = PathResolver_UpdateIterator(pResolver, user, &iter, &pResolver->pathComponent);
        if (err == ENOENT) {
            // Scan until we find the start of the next path component
            while(pPath[pi] == '/') pi++;

            if ((mode == kPathResolutionMode_TargetOrAncestor)
                || (mode == kPathResolutionMode_TargetOrParent && pPath[pi] == '\0')) {
                // Some path component doesn't exist and the caller is asking for
                // ancestor or parent information. Let's return the ancestor that
                // does exist. Note that we do a move of ownership here.
                pResult->inode = iter.inode;
                pResult->fileSystem = iter.fileSystem;
                pResult->pathSuffix = &pPath[pathSuffixStartIdx];

                return ENOENT;
            }

            throw(err);
        } else if (err != EOK) {
            throw(err);
        }


        // We're done if we've reached the end of the path. Otherwise continue
        // with the updated iterator
        if (pPath[pi] == '\0') {
            break;
        }
    }

    // Note that we move (ownership) of the target node & filesystem from the
    // iterator to the result
    pResult->inode = iter.inode;
    pResult->fileSystem = iter.fileSystem;
    pResult->pathSuffix = &pPath[pi];
    return EOK;

catch:
    InodeIterator_Deinit(&iter);
    return err;
}
