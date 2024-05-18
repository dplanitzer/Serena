//
//  PathResolver.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "PathResolver.h"
#include "FilesystemManager.h"

static void PathResolverResult_Init(PathResolverResult* _Nonnull self)
{
    self->parent = NULL;
    self->target = NULL;
    self->lastPathComponent.name = NULL;
    self->lastPathComponent.count = 0;
}

void PathResolverResult_Deinit(PathResolverResult* _Nonnull self)
{
    if (self->target) {
        Inode_Relinquish(self->target);
        self->target = NULL;
    }
    if (self->parent) {
        Inode_Relinquish(self->parent);
        self->parent = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: PathResolver
////////////////////////////////////////////////////////////////////////////////

void PathResolver_Init(PathResolverRef _Nonnull self, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, User user)
{
    self->rootDirectory = pRootDir;
    self->workingDirectory = pWorkingDir;
    self->user = user;
}

void PathResolver_Deinit(PathResolverRef _Nullable self)
{
    self->rootDirectory = NULL;
    self->workingDirectory = NULL;
}

// Acquires the parent directory of the directory 'pDir'. Returns 'pDir' again
// if that inode is the path resolver's root directory. Returns a suitable error
// code and leaves the iterator unchanged if an error (eg access denied) occurs.
// Walking up means resolving a path component of the form '..'.
static errno_t PathResolver_AcquireParentDirectory(PathResolverRef _Nonnull self, InodeRef _Nonnull pDir, InodeRef _Nullable * _Nonnull pOutParentDir)
{
    decl_try_err();
    InodeRef _Locked pParentDir = NULL;
    InodeRef _Locked pMountingDir = NULL;
    InodeRef _Locked pParentOfMountingDir = NULL;
    FilesystemRef pDirFS = Inode_GetFilesystem(pDir);

    // Do not walk past the root directory
    if (Inode_Equals(pDir, self->rootDirectory)) {
        *pOutParentDir = Inode_Reacquire(pDir);
        return EOK;
    }


    try(Filesystem_AcquireNodeForName(pDirFS, pDir, &kPathComponent_Parent, self->user, NULL, &pParentDir));

    if (!Inode_Equals(pDir, pParentDir)) {
        // We're moving to a parent directory in the same file system
        *pOutParentDir = pParentDir;
    }
    else {
        Inode_Relinquish(pParentDir);
        pParentDir = NULL;


        // The 'pDir' node is the root of a file system that is mounted somewhere
        // below the root directory. We need to find the node in the parent
        // file system that is mounting 'pDir' and we then need to find the
        // parent of this inode. Note that such a parent always exists and that it
        // is necessarily in the same parent file system in which the mounting node
        // is (because you can not mount a file system on the root node of another
        // file system).
        try(FilesystemManager_AcquireNodeMountingFilesystem(gFilesystemManager, pDirFS, &pMountingDir));
        try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pMountingDir), pMountingDir, &kPathComponent_Parent, self->user, NULL, &pParentOfMountingDir));

        Inode_Relinquish(pMountingDir);
        *pOutParentDir = pParentOfMountingDir;
    }

    return EOK;

catch:
    Inode_Relinquish(pMountingDir);
    *pOutParentDir = NULL;
    return err;
}

// Acquires the child node 'pName' of the directory 'pDir' and returns EOK if this
// works out. Otherwise returns a suitable error and returns NULL as the node.
// This function handles the case that we want to walk down the filesystem tree
// (meaning that the given path component is a file or directory name and
// neither '.' nor '..').
static errno_t PathResolver_AcquireChildNode(PathResolverRef _Nonnull self, InodeRef _Nonnull pDir, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nonnull pOutChildNode)
{
    decl_try_err();
    InodeRef _Locked pChildNode = NULL;
    FilesystemRef pMountedFileSys = NULL;

    // Ask the filesystem for the inode that is named by the tuple (pDir, pName)
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pDir), pDir, pName, self->user, pDirInsHint, &pChildNode));


    // This can only happen if the filesystem is in a corrupted state.
    if (Inode_Equals(pDir, pChildNode)) {
        Inode_Relinquish(pChildNode);
        throw(EIO);
    }


    // Check whether the new inode is a mountpoint. If not then we just return
    // the acquired node as is. Otherwise we'll have to look up the root directory
    // of the mounted filesystem.
    pMountedFileSys = FilesystemManager_CopyFilesystemMountedAtNode(gFilesystemManager, pChildNode);

    if (pMountedFileSys == NULL) {
        *pOutChildNode = pChildNode;
    }
    else {
        InodeRef pMountedFileSysRootNode;

        try(Filesystem_AcquireRootNode(pMountedFileSys, &pMountedFileSysRootNode));
        *pOutChildNode = pMountedFileSysRootNode;
        Inode_Relinquish(pChildNode);
        Object_Release(pMountedFileSys);
    }
    return EOK;

catch:
    Object_Release(pMountedFileSys);
    return err;
}

static errno_t get_next_path_component(const char* _Nonnull pPath, size_t* _Nonnull pi, PathComponent* _Nonnull pc, bool* _Nonnull pOutIsLastPathComponent)
{
    decl_try_err();
    size_t i = *pi;

    // Skip over '/' character(s)
    while (i < kMaxPathLength && pPath[i] == '/') {
        i++;
    }
    if (i >= kMaxPathLength) {
        throw(ENAMETOOLONG);
    }


    // A path with trailing slashes like this:
    // x/y////
    // is treated as if it would be a path of the form:
    // x/y/.
    if (i > *pi && pPath[i] == '\0') {
        pc->name = ".";
        pc->count = 1;
        *pi = i;
        *pOutIsLastPathComponent = true;
        return EOK;
    }
        

    // Pick up the next path component name
    const size_t is = i;
    while (i < kMaxPathLength && pPath[i] != '\0' && pPath[i] != '/') {
        i++;
    }
    const size_t nc = i - is;
    if (i >= kMaxPathLength || nc >= kMaxPathComponentLength) {
        throw(ENAMETOOLONG);
    }

    pc->name = &pPath[is];
    pc->count = nc;
    *pi = i;
    *pOutIsLastPathComponent = (pPath[i] == '\0') ? true : false;
    return EOK;

catch:
    pc->name = "";
    pc->count = 0;
    *pi = i;
    *pOutIsLastPathComponent = false;
    return err;
}

// Atomically looks up the name of the node 'idOfNodeToLookup' in the directory
// 'pDir' and returns it in 'pc' if successful. This lookup may fail with ENOENT
// which happens if the node has been removed from the directory. It may fail
// with EACCESS if the directory lacks search and read permissions for the user
// 'user'.
static errno_t get_name_of_node(InodeId idOfNodeToLookup, InodeRef _Nonnull pDir, User user, MutablePathComponent* _Nonnull pc)
{
    decl_try_err();

    Inode_Lock(pDir);
    err = Filesystem_GetNameOfNode(Inode_GetFilesystem(pDir), pDir, idOfNodeToLookup, user, pc);
    Inode_Unlock(pDir);
    return err;
}

errno_t PathResolver_GetDirectoryPath(PathResolverRef _Nonnull self, InodeRef _Nonnull pStartDir, char* _Nonnull  pBuffer, size_t bufferSize)
{
    decl_try_err();
    MutablePathComponent pc;
    InodeRef pCurDir = Inode_Reacquire(pStartDir);

    if (bufferSize < 1) {
        throw(EINVAL);
    }

    // We walk up the filesystem from 'pNode' to the global root of the filesystem
    // and we build the path right aligned in the user provided buffer. We'll move
    // the path such that it'll start at 'pBuffer' once we're done.
    char* p = &pBuffer[bufferSize - 1];
    *p = '\0';

    while (!Inode_Equals(pCurDir, self->rootDirectory)) {
        const InodeId childInodeIdToLookup = Inode_GetId(pCurDir);
        InodeRef pParentDir = NULL;

        try(PathResolver_AcquireParentDirectory(self, pCurDir, &pParentDir));
        Inode_Relinquish(pCurDir);
        pCurDir = pParentDir; pParentDir = NULL;

        pc.name = pBuffer;
        pc.count = 0;
        pc.capacity = p - pBuffer;
        try(get_name_of_node(childInodeIdToLookup, pCurDir, self->user, &pc));

        p -= pc.count;
        memcpy(p, pc.name, pc.count);

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
    
    memcpy(pBuffer, p, &pBuffer[bufferSize] - p);
    Inode_Relinquish(pCurDir);
    return EOK;

catch:
    Inode_Relinquish(pCurDir);
    pBuffer[0] = '\0';
    return err;
}

// Looks up the inode named by the given path. The path may be relative or absolute.
errno_t PathResolver_AcquireNodeForPath(PathResolverRef _Nonnull self, PathResolverMode mode, const char* _Nonnull pPath, PathResolverResult* _Nonnull pResult)
{
    decl_try_err();
    DirectoryEntryInsertionHint* pInsertionHint;
    InodeRef pPrevNode = NULL;
    InodeRef pCurNode = NULL;
    InodeRef pNextNode = NULL;
    PathComponent pc;
    size_t pi = 0;
    bool isLastPathComponent = false;

    PathResolverResult_Init(pResult);
    pInsertionHint = (mode == kPathResolverMode_TargetOrParent) ? &pResult->insertionHint : NULL;

    if (pPath[0] == '\0') {
        return ENOENT;
    }


    // Start with the root directory if the path starts with a '/' and the
    // current working directory otherwise
    InodeRef pStartNode = (pPath[0] == '/') ? self->rootDirectory : self->workingDirectory;
    pCurNode = Inode_Reacquire(pStartNode);


    // Iterate through the path components, looking up the inode that corresponds
    // to the current path component. Stop once we hit the end of the path.
    // Note that:
    // * lookup of '.' can not fail with ENOENT because it's the same as the current directory
    // * lookup of '..' can not fail with ENOENT because every directory has a parent (parent of root is root itself)
    // * lookup of a named entry can fail with ENOENT
    for (;;) {
        try(get_next_path_component(pPath, &pi, &pc, &isLastPathComponent));

        if (pc.count == 0) {
            break;
        }


        // The current directory better be an actual directory
        if (!Inode_IsDirectory(pCurNode)) {
            throw(ENOTDIR);
        }

        if (pc.count == 1 && pc.name[0] == '.') {
            // pCurNode and pPrevNode do not change
            continue;
        }
        else if (pc.count == 2 && pc.name[0] == '.' && pc.name[1] == '.') {
            try(PathResolver_AcquireParentDirectory(self, pCurNode, &pNextNode));

            Inode_Relinquish(pPrevNode);
            pPrevNode = NULL;

            Inode_Relinquish(pCurNode);
            pCurNode = pNextNode;
            pNextNode = NULL;
        }
        else {
            try(PathResolver_AcquireChildNode(self, pCurNode, &pc, pInsertionHint, &pNextNode));

            pPrevNode = pCurNode;
            pCurNode = pNextNode;
            pNextNode = NULL;
        }
    }


    // Make sure that the last path component is a directory if the path ends
    // with a trailing slash. The pi - 1 index points to the trailing slash if
    // it exists.
    if (!Inode_IsDirectory(pCurNode) && pPath[pi - 1] == '/') {
        throw(ENOTDIR);
    }


    // Note that we move ownership of the target node to the result structure
    pResult->target = pCurNode;

    if (mode == kPathResolverMode_TargetAndParent || mode == kPathResolverMode_OptionalTargetAndParent) {
        if (pPrevNode) {
            pResult->parent = pPrevNode;
        }
        else {
            try(PathResolver_AcquireParentDirectory(self, pResult->target, &pResult->parent));
        }
        pResult->lastPathComponent = pc;
    }
    else {
        Inode_Relinquish(pPrevNode);
    }
    return EOK;

catch:
    Inode_Relinquish(pPrevNode);

    if (err == ENOENT && isLastPathComponent &&
        (mode == kPathResolverMode_TargetOrParent || mode == kPathResolverMode_OptionalTargetAndParent)) {
            pResult->parent = pCurNode;
            pResult->lastPathComponent = pc;
            return EOK;
    }

    Inode_Relinquish(pCurNode);
    return err;
}
