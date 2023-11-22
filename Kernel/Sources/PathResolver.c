//
//  PathResolver.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "PathResolver.h"
#include "FilesystemManager.h"

extern ErrorCode MakePathFromInode(InodeRef _Nonnull pNode, InodeRef _Nonnull pRootDir, User user, Character* pBuffer, ByteCount bufferSize);

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

ErrorCode PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* pPath)
{
    decl_try_err();
    InodeRef pNode;
    Bool isChildOfOldRoot;

    try(PathResolver_CopyNodeForPath(pResolver, pPath, user, &pNode));
    if (!Inode_IsDirectory(pNode)) {
        throw(ENOTDIR);
    }
    try(Inode_IsChildOfNode(pNode, pResolver->rootDirectory, user, &isChildOfOldRoot));
    if (!isChildOfOldRoot) {
        throw(ENOENT);
    }

    if (pResolver->rootDirectory != pNode) {
        Object_Release(pResolver->rootDirectory);
        pResolver->rootDirectory = pNode;
    }

    return EOK;

catch:
    Object_Release(pNode);
    return err;
}

ErrorCode PathResolver_GetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, Character* pBuffer, ByteCount bufferSize)
{
    return MakePathFromInode(pResolver->currentWorkingDirectory, pResolver->rootDirectory, user, pBuffer, bufferSize);
}

ErrorCode PathResolver_SetCurrentWorkingDirectoryPath(PathResolverRef _Nonnull pResolver, User user, const Character* _Nonnull pPath)
{
    decl_try_err();
    InodeRef pNode;

    try(PathResolver_CopyNodeForPath(pResolver, pPath, user, &pNode));
    if (!Inode_IsDirectory(pNode)) {
        throw(ENOTDIR);
    }

    if (pResolver->currentWorkingDirectory != pNode) {
        Object_Release(pResolver->currentWorkingDirectory);
        pResolver->currentWorkingDirectory = pNode;
    }

    return EOK;

catch:
    return err;
}

static ErrorCode PathResolver_UpdateIterator(PathResolverRef _Nonnull pResolver, InodeIterator* _Nonnull pIter, const PathComponent* pComponent, User user)
{
    // The current directory better be an actual directory
    if (!Inode_IsDirectory(pIter->inode)) {
        return ENOTDIR;
    }


    // Handle "." and ".."
    if (pComponent->count <= 2 && pComponent->name[0] == '.') {
        if (pComponent->count == 1) {
            return EOK;
        }

        if (pComponent->name[1] == '.') {
            InodeRef parentNode;
            ErrorCode err = Filesystem_CopyParentOfNode(pIter->fileSystem, pIter->inode, user, &parentNode);

            if (err == EOK) {
                // We're moving to a parent node in the same filesystem
                Object_Release(pIter->inode);
                pIter->inode = parentNode;
                return EOK;
            }

            if (err == ENOENT) {
                // The pIter->inode is the root of a filesystem. Loop back to the
                // global root if pIter->inode is the global root. So "/.." is
                // the same as "/."
                if (pIter->inode == pResolver->rootDirectory) {
                    return EOK;
                }

                // The pIter->inode is the root of a filesystem that is mounted
                // somewhere below the global filesystem root. We need to find
                // the node in the parent filesystem that is mounting pIter->inode
                // and we the need to find the parent of this inode. Note that such
                // a parent always exists and that it is necessarily in the same
                // parent filesystem in which the mounting node is (because you
                // can not mount a filesystem on the root node of another filesystem).
                InodeRef pMountingDir;
                FilesystemRef pMountingFilesystem;

                err = FilesystemManager_CopyNodeAndFilesystemMountingFilesystemId(gFilesystemManager, pIter->fsid, &pMountingDir, &pMountingFilesystem);
                if (err != EOK) {
                    return err;
                }

                try_bang(Filesystem_CopyParentOfNode(pMountingFilesystem, pMountingDir, user, &parentNode));

                Object_Release(pIter->inode);
                pIter->inode = parentNode;
                Object_Release(pIter->fileSystem);
                pIter->fileSystem = pMountingFilesystem;
                pIter->fsid = Filesystem_GetId(pMountingFilesystem);

                return EOK;
            }

            return err;
        }
    }


    // Ask the current filesystem for the inode that is named by the tuple
    // (parent-inode, path-component)
    InodeRef childNode;
    const ErrorCode err = Filesystem_CopyNodeForName(pIter->fileSystem, pIter->inode, pComponent, user, &childNode);
    if (err != EOK) {
        return err;
    }


    // Just return the node we looked up without changing the filesystem if the
    // node is not a mount point
    const FilesystemId mountedFsId = Inode_GetMountedFilesystemId(childNode);
    if (mountedFsId == 0) {
        Object_Release(pIter->inode);
        pIter->inode = childNode;
        return EOK;
    }


    // The node is a mount point. Figure out what the mounted filesystem is
    // and return the root node of this filesystem
    FilesystemRef mountedFilesystem = FilesystemManager_CopyFilesystemForId(gFilesystemManager, mountedFsId);
    if (mountedFilesystem == NULL) {
        return ENOENT;
    }

    Object_Release(childNode);
    Object_Release(pIter->inode);
    pIter->inode = Filesystem_CopyRootNode(mountedFilesystem);
    Object_Release(pIter->fileSystem);
    pIter->fileSystem = mountedFilesystem;
    pIter->fsid = Filesystem_GetId(mountedFilesystem);

    return EOK;
}

ErrorCode PathResolver_CopyNodeForPath(PathResolverRef _Nonnull pResolver, const Character* _Nonnull pPath, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeIterator iter;
    Int pi = 0;

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
        Int ni = 0;
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


        // Ask the current namespace for the inode that is named by the tuple
        // (parent-inode, path-component)
        try(PathResolver_UpdateIterator(pResolver, &iter, &pResolver->pathComponent, user));


        // We're done if we've reached the end of the path. Otherwise continue
        // continue with the updated iterator
        if (pPath[pi] == '\0') {
            break;
        }
    }

    *pOutNode = Object_RetainAs(iter.inode, Inode);
    InodeIterator_Deinit(&iter);
    return EOK;

catch:
    InodeIterator_Deinit(&iter);
    *pOutNode = NULL;
    return err;
}


ErrorCode MakePathFromInode(InodeRef _Nonnull pNode, InodeRef _Nonnull pRootDir, User user, Character* pBuffer, ByteCount bufferSize)
{
    decl_try_err();
    InodeRef pCurNode = Object_RetainAs(pNode, Inode);
    FilesystemRef pCurFilesystem = Inode_CopyFilesystem(pCurNode);

    if (bufferSize <= 0) {
        throw(EINVAL);
    }
    if (pCurFilesystem == NULL) {
        throw(EACCESS);
    }

    // We walk up the filesystem from 'pNode' to the global root of the filesystem
    // and we build the path right aligned in the user provided buffer. We'll move
    // the path such that it'll start at 'pBuffer' once we're done.
    MutablePathComponent pathComponent;
    Character* p = &pBuffer[bufferSize - 1];
    *p = '\0';

    while (true) {
        InodeRef pParentNode;
        err = Filesystem_CopyParentOfNode(pCurFilesystem, pCurNode, user, &pParentNode);
        if (err == ENOENT) {
            if (pCurNode == pRootDir) {
                break;
            }

            InodeRef pMountingDir;
            FilesystemRef pMountingFilesystem;

            try(FilesystemManager_CopyNodeAndFilesystemMountingFilesystemId(gFilesystemManager, Inode_GetFilesystemId(pCurNode), &pMountingDir, &pMountingFilesystem));
            try_bang(Filesystem_CopyParentOfNode(pMountingFilesystem, pMountingDir, user, &pParentNode));

            Object_Release(pCurFilesystem);
            pCurFilesystem = pMountingFilesystem;
        } else if (err != EOK) {
            throw(err);
        }

        pathComponent.name = pBuffer;
        pathComponent.count = 0;
        pathComponent.capacity = p - pBuffer;
        try(Filesystem_GetNameOfNode(pCurFilesystem, pParentNode, pCurNode, user, &pathComponent));

        p -= pathComponent.count;
        Bytes_CopyRange(p, pathComponent.name, pathComponent.count);

        Object_Release(pCurNode);
        pCurNode = pParentNode;

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
    Object_Release(pCurFilesystem);
    Object_Release(pCurNode);

    return EOK;

catch:
    Object_Release(pCurFilesystem);
    Object_Release(pCurNode);
    pBuffer[0] = '\0';

    return err;
}
