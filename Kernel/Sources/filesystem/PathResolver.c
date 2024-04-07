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
    self->inode = NULL;
    self->filesystem = NULL;
    self->lastPathComponent.name = NULL;
    self->lastPathComponent.count = 0;
}

void PathResolverResult_Deinit(PathResolverResult* _Nonnull self)
{
    if (self->inode) {
        Filesystem_RelinquishNode(self->filesystem, self->inode);
        self->inode = NULL;
    }
    if (self->filesystem) {
        Object_Release(self->filesystem);
        self->filesystem = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: InodeIterator
////////////////////////////////////////////////////////////////////////////////

// Tracks our current position in the global filesystem
typedef struct InodeIterator {
    InodeRef _Nonnull _Locked   inode;
    FilesystemRef _Nonnull      filesystem;
} InodeIterator;


static void InodeIterator_Init(InodeIterator* _Nonnull self, FilesystemRef _Nonnull pFirstFs, InodeRef _Nonnull pFirstNode)
{
    self->filesystem = Object_RetainAs(pFirstFs, Filesystem);
    self->inode = Filesystem_ReacquireNode(self->filesystem, pFirstNode);
}

static void InodeIterator_Deinit(InodeIterator* _Nonnull self)
{
    if (self->inode) {
        Filesystem_RelinquishNode(self->filesystem, self->inode);
        self->inode = NULL;
    }
    if (self->filesystem) {
        Object_Release(self->filesystem);
        self->filesystem = NULL;
    }
}

// Takes ownership of 'pNewNode' and expects that 'pNewNode' and the current node
// of the iterator are different. Updates the inode only (assumption is that the
// new and the old inode are located on the same filesystem).
static void InodeIterator_UpdateWithNodeOnly(InodeIterator* self, InodeRef _Nonnull pNewNode)
{
    Filesystem_RelinquishNode(self->filesystem, self->inode);
    self->inode = pNewNode;
}

// Takes ownership of 'pNewNode' and expects that 'pNewNode' and the current node
// of the iterator are different. Updates both the inode and the filesystem
// information.
static void InodeIterator_Update(InodeIterator* self, InodeRef _Nonnull pNewNode)
{
    Filesystem_RelinquishNode(self->filesystem, self->inode);
    self->inode = pNewNode;

    FilesystemRef pNewFileSys = Inode_CopyFilesystem(pNewNode);
    Object_Release(self->filesystem);
    self->filesystem = pNewFileSys;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: PathResolver
////////////////////////////////////////////////////////////////////////////////

static errno_t PathResolver_UpdateIteratorWalkingUp(PathResolverRef _Nonnull self, User user, InodeIterator* _Nonnull pIter);


// Creates a path resolver that allows you to resolve paths in the given root
// filesystem. The resolver acquires the root node and uses it as its root
// and working directory.
errno_t PathResolver_Create(FilesystemRef _Nonnull pRootFs, PathResolverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    PathResolverRef self;
    InodeRef pRootDir = NULL;

    try(Filesystem_AcquireRootNode(pRootFs, &pRootDir));

    try(kalloc_cleared(sizeof(PathResolver), (void**)&self));
    self->rooFilesystem = Object_RetainAs(pRootFs, Filesystem);
    self->rootDirectory = pRootDir;
    self->workingFilesystem = Object_RetainAs(pRootFs, Filesystem);
    self->workingDirectory = Filesystem_ReacquireUnlockedNode(pRootFs, pRootDir);
    self->pathComponent.name = self->nameBuffer;
    self->pathComponent.count = 0;
    *pOutSelf = self;
    
    return EOK;

catch:
    if (pRootDir) {
        Filesystem_RelinquishNode(pRootFs, pRootDir);
    }
    *pOutSelf = NULL;
    return err;
}

// Creates a copy of the given path resolver. The copy will maintain its own
// separate root and working directory inode references.
errno_t PathResolver_CreateCopy(PathResolverRef _Nonnull pOther, PathResolverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    PathResolverRef self;

    try(kalloc_cleared(sizeof(PathResolver), (void**)&self));
    self->rooFilesystem = Object_RetainAs(pOther->rooFilesystem, Filesystem);
    self->rootDirectory = Filesystem_ReacquireUnlockedNode(self->rooFilesystem, pOther->rootDirectory);
    self->workingFilesystem = Object_RetainAs(pOther->workingFilesystem, Filesystem);
    self->workingDirectory = Filesystem_ReacquireUnlockedNode(self->workingFilesystem, pOther->workingDirectory);
    self->pathComponent.name = self->nameBuffer;
    self->pathComponent.count = 0;
    *pOutSelf = self;
    
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void PathResolver_Destroy(PathResolverRef _Nullable self)
{
    if (self) {
        if (self->rootDirectory) {
            Filesystem_RelinquishNode(self->rooFilesystem, self->rootDirectory);
            self->rootDirectory = NULL;
            Object_Release(self->rooFilesystem);
            self->rooFilesystem = NULL;
        }
        if (self->workingDirectory) {
            Filesystem_RelinquishNode(self->workingFilesystem, self->workingDirectory);
            self->workingDirectory = NULL;
            Object_Release(self->workingFilesystem);
            self->workingFilesystem = NULL;
        }

        kfree(self);
    }
}

static errno_t PathResolver_SetDirectoryPath(PathResolverRef _Nonnull self, User user, const char* _Nonnull pPath, FilesystemRef _Nonnull * _Nonnull pFsToAssign, InodeRef _Nonnull * _Nonnull pDirToAssign)
{
    decl_try_err();
    PathResolverResult r;

    // Get the inode that represents the new directory
    try(PathResolver_AcquireNodeForPath(self, kPathResolutionMode_TargetOnly, pPath, user, &r));


    // Make sure that it is actually a directory
    if (!Inode_IsDirectory(r.inode)) {
        throw(ENOTDIR);
    }


    // Make sure that we do have search permission on the last path component (directory)
    try(Filesystem_CheckAccess(r.filesystem, r.inode, user, kFilePermission_Execute));


    // Remember the new inode as our new directory
    if (Inode_GetId(*pDirToAssign) != Inode_GetId(r.inode)) {
        Filesystem_RelinquishNode(*pFsToAssign, *pDirToAssign);
        *pDirToAssign = r.inode;
        r.inode = NULL;
    }
    if (Inode_GetFilesystemId(*pDirToAssign) != Inode_GetFilesystemId(r.inode)) {
        Object_Release(*pFsToAssign);
        *pFsToAssign = r.filesystem;
        r.filesystem = NULL;
    }

catch:
    PathResolverResult_Deinit(&r);
    return err;
}

errno_t PathResolver_SetRootDirectoryPath(PathResolverRef _Nonnull self, User user, const char* _Nonnull pPath)
{
    return PathResolver_SetDirectoryPath(self, user, pPath, &self->rooFilesystem, &self->rootDirectory);
}

// Returns true if the given node represents the resolver's root directory.
bool PathResolver_IsRootDirectory(PathResolverRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    return Inode_GetFilesystemId(self->rootDirectory) == Inode_GetFilesystemId(pNode)
        && Inode_GetId(self->rootDirectory) == Inode_GetId(pNode);
}

errno_t PathResolver_GetWorkingDirectoryPath(PathResolverRef _Nonnull self, User user, char* _Nonnull  pBuffer, size_t bufferSize)
{
    InodeIterator iter;
    decl_try_err();

    InodeIterator_Init(&iter, self->workingFilesystem, self->workingDirectory);

    if (bufferSize < 1) {
        throw(EINVAL);
    }

    // We walk up the filesystem from 'pNode' to the global root of the filesystem
    // and we build the path right aligned in the user provided buffer. We'll move
    // the path such that it'll start at 'pBuffer' once we're done.
    MutablePathComponent pathComponent;
    char* p = &pBuffer[bufferSize - 1];
    *p = '\0';

    while (iter.inode != self->rootDirectory) {
        const InodeId childInodeIdToLookup = Inode_GetId(iter.inode);

        try(PathResolver_UpdateIteratorWalkingUp(self, user, &iter));

        pathComponent.name = pBuffer;
        pathComponent.count = 0;
        pathComponent.capacity = p - pBuffer;
        try(Filesystem_GetNameOfNode(iter.filesystem, iter.inode, childInodeIdToLookup, user, &pathComponent));

        p -= pathComponent.count;
        memcpy(p, pathComponent.name, pathComponent.count);

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
    InodeIterator_Deinit(&iter);
    return EOK;

catch:
    InodeIterator_Deinit(&iter);
    pBuffer[0] = '\0';
    return err;
}

errno_t PathResolver_SetWorkingDirectoryPath(PathResolverRef _Nonnull self, User user, const char* _Nonnull pPath)
{
    return PathResolver_SetDirectoryPath(self, user, pPath, &self->workingFilesystem, &self->workingDirectory);
}

// Updates the given inode iterator with the parent node of the node to which the
// iterator points. Returns the iterator's inode itself if that inode is the path
// resolver's root directory. Returns a suitable error code and leaves the iterator
// unchanged if an error (eg access denied) occurs.
static errno_t PathResolver_UpdateIteratorWalkingUp(PathResolverRef _Nonnull self, User user, InodeIterator* _Nonnull pIter)
{
    InodeRef _Locked pParentNode = NULL;
    InodeRef _Locked pMountingDir = NULL;
    InodeRef _Locked pParentOfMountingDir = NULL;
    FilesystemRef pMountingFilesystem = NULL;
    decl_try_err();

    // Nothing to do if the iterator points to our root node
    if (Inode_Equals(pIter->inode, self->rootDirectory)) {
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
static errno_t PathResolver_UpdateIteratorWalkingDown(PathResolverRef _Nonnull self, User user, InodeIterator* _Nonnull pIter, const PathComponent* pComponent)
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
static errno_t PathResolver_UpdateIterator(PathResolverRef _Nonnull self, User user, InodeIterator* _Nonnull pIter, const PathComponent* pComponent)
{
    // The current directory better be an actual directory
    if (!Inode_IsDirectory(pIter->inode)) {
        return ENOTDIR;
    }


    // Walk up the filesystem tree if the path component is "..", sideways if
    // the path component is "." and down if it is any other name.
    if (pComponent->count == 2 && pComponent->name[0] == '.' && pComponent->name[1] == '.') {
        return PathResolver_UpdateIteratorWalkingUp(self, user, pIter);
    }
    else {
        return PathResolver_UpdateIteratorWalkingDown(self, user, pIter, pComponent);
    }
}

// Looks up the inode named by the given path. The path may be relative or absolute.
errno_t PathResolver_AcquireNodeForPath(PathResolverRef _Nonnull self, PathResolutionMode mode, const char* _Nonnull pPath, User user, PathResolverResult* _Nonnull pResult)
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
    FilesystemRef pStartingFs;
    InodeRef pStartingDir;
    if (pPath[0] == '/') {
        pStartingFs = self->rooFilesystem;
        pStartingDir = self->rootDirectory;
    } else {
        pStartingFs = self->workingFilesystem;
        pStartingDir = self->workingDirectory;
    }


    // Create our inode iterator
    InodeIterator_Init(&iter, pStartingFs, pStartingDir);


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
            self->nameBuffer[ni++] = pPath[pi++];
        }


        // Treat a path that ends in a trailing '/' as if it would be "/."
        if (ni == 0) {
            self->nameBuffer[0] = '.'; ni = 1;
        }
        self->pathComponent.count = ni;


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
        try(PathResolver_UpdateIterator(self, user, &iter, &self->pathComponent));


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
