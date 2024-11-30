//
//  FileHierarchy.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/28/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileHierarchy.h"
#include <dispatcher/SELock.h>


typedef struct FSDownlink {
    ListNode                node;
    
    // Parent FS and directory
    FilesystemId            fsid;
    InodeId                 inid;

    // Child FS and directory
    FilesystemRef _Nonnull  childFS;            // Owning
    InodeRef _Nonnull       childDirectory;     // Owning
} FSDownlink;

typedef struct FSUplink {
    ListNode                        node;
    
    // Child FS and directory
    FilesystemId                    fsid;
    InodeId                         inid;

    // Parent FS and directory
    FilesystemRef _Nonnull _Weak    parentFS;
    InodeRef _Nonnull _Weak         parentDirectory;
} FSUplink;

final_class_ivars(FileHierarchy, Object,
    FilesystemRef   rootFilesystem;     // constant over the hierarchy lifetime
    InodeRef        rootDir;            // constant over the hierarchy lifetime
    FilesystemId    rootFsId;
    InodeId         rootInId;

    SELock          lock;
    List            fsUplinks;
    List            fsDownlinks;
);

static errno_t FileHierarchy_AcquireParentDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeRef _Nonnull rootDir, User user, InodeRef _Nullable * _Nonnull pOutParentDir);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: ResolvedPath
////////////////////////////////////////////////////////////////////////////////

static void ResolvedPath_Init(ResolvedPath* _Nonnull self)
{
    self->inode = NULL;
    self->lastPathComponent.name = NULL;
    self->lastPathComponent.count = 0;
}

void ResolvedPath_Deinit(ResolvedPath* _Nonnull self)
{
    if (self->inode) {
        Inode_Relinquish(self->inode);
        self->inode = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: FileHierarchy
////////////////////////////////////////////////////////////////////////////////

errno_t FileHierarchy_Create(FilesystemRef _Nonnull rootFS, FileHierarchyRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FileHierarchyRef self;

    try(Object_Create(FileHierarchy, &self));
    SELock_Init(&self->lock);
    List_Init(&self->fsUplinks);
    List_Init(&self->fsDownlinks);
    self->rootFilesystem = Object_RetainAs(rootFS, Filesystem);
    try(Filesystem_AcquireRootDirectory(rootFS, &self->rootDir));
    self->rootFsId = Filesystem_GetId(self->rootFilesystem);
    self->rootInId = Inode_GetId(self->rootDir);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    Object_Release(self);
    return err;
}

void FileHierarchy_deinit(FileHierarchyRef _Nullable self)
{
    if (self->rootDir) {
        Inode_Relinquish(self->rootDir);
        self->rootDir = NULL;
    }
    if (self->rootFilesystem) {
        Object_Release(self->rootFilesystem);
        self->rootFilesystem = NULL;
    }
    List_Deinit(&self->fsDownlinks);
    List_Deinit(&self->fsUplinks);
    SELock_Deinit(&self->lock);
}

// Returns a strong reference to the root filesystem of the given file hierarchy.
FilesystemRef FileHierarchy_CopyRootFilesystem(FileHierarchyRef _Nonnull self)
{
    return Object_RetainAs(self->rootFilesystem, Filesystem);
}

// Returns the root directory of the given file hierarchy.
InodeRef _Nonnull FileHierarchy_AcquireRootDirectory(FileHierarchyRef _Nonnull self)
{
    return Filesystem_ReacquireNode(self->rootFilesystem, self->rootDir);
}

// Attaches the root directory of the filesystem 'fs' to the directory 'atDir'. 'atDir'
// must be a member of this file hierarchy and may not already have another filesystem
// attached to it.
errno_t FileHierarchy_AttachFilesystem(FileHierarchyRef _Nonnull self, FilesystemRef _Nonnull fs, InodeRef _Nonnull atDir)
{
    decl_try_err();
    FSUplink* uplink = NULL;
    FSDownlink* downlink = NULL;
    InodeRef fsRootDir = NULL;

    if (!Inode_IsDirectory(atDir)) {
        return ENOTDIR;
    }

    Object_Retain(fs);
    try(Filesystem_AcquireRootDirectory(fs, &fsRootDir));


    try_bang(SELock_LockExclusive(&self->lock));

    // XXX check that 'atDir' filesystem is in our hierarchy
    // XXX check that 'atDir' isn't already a mountpoint in our hierarchy

    try(kalloc_cleared(sizeof(FSUplink), (void**)&uplink));
    uplink->fsid = Filesystem_GetId(fs);
    uplink->inid = Inode_GetId(fsRootDir);
    uplink->parentFS = Inode_GetFilesystem(atDir);
    uplink->parentDirectory = atDir;


    try(kalloc_cleared(sizeof(FSDownlink), (void**)&downlink));
    downlink->fsid = Inode_GetFilesystemId(atDir);
    downlink->inid = Inode_GetId(atDir);
    downlink->childFS = fs;
    downlink->childDirectory = fsRootDir;

    List_InsertBeforeFirst(&self->fsUplinks, &uplink->node);
    List_InsertBeforeFirst(&self->fsDownlinks, &downlink->node);

    try_bang(SELock_Unlock(&self->lock));
    return EOK;

catch:
    kfree(downlink);
    kfree(uplink);
    try_bang(SELock_Unlock(&self->lock));

    Inode_Relinquish(fsRootDir);
    Object_Release(fs);
    return err;
}

errno_t FileHierarchy_DetachFilesystem(FileHierarchyRef _Nonnull self, FilesystemRef _Nonnull fs)
{
    decl_try_err();

    // XXX implement me
    return err;
}

// Returns true if the given (directory) inode is an attachment point for another
// filesystem.
bool FileHierarchy_IsAttachmentPoint(FileHierarchyRef _Nonnull self, InodeRef _Nonnull inode)
{
    const FilesystemId targetFsId = Inode_GetFilesystemId(inode);
    const InodeId targetInId = Inode_GetId(inode);
    bool r = false;

    SELock_LockShared(&self->lock);
    List_ForEach(&self->fsDownlinks, FSDownlink,
        if (pCurNode->fsid == targetFsId && pCurNode->inid == targetInId) {
            r = true;
            break;
        }
    );
    SELock_Unlock(&self->lock);
    return r;
}


// Acquires the inode that is mounting the given directory. A suitable error
// and NULL is returned if the given directory is not mounted (anymore) or some
// other problem is detected.
// EOK and NULL are returned if 'dir' is the root directory of the hierarchy.
static errno_t FileHierarchy_AcquireDirectoryMountingDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull dir, InodeRef _Nullable _Locked * _Nonnull pOutMountingDir)
{
    decl_try_err();
    const InodeId targetInId = Inode_GetId(dir);
    const FilesystemId targetFsId = Inode_GetFilesystemId(dir);

    List_ForEach(&self->fsUplinks, FSUplink,
        if (pCurNode->fsid == targetFsId && pCurNode->inid == targetInId) {
            *pOutMountingDir = Inode_Reacquire(pCurNode->parentDirectory);
            return EOK;
        }
    )

    if (targetFsId == self->rootFsId && targetInId == self->rootInId) {
        *pOutMountingDir = NULL;
        return EOK;
    }

    *pOutMountingDir = NULL;
    return ENOENT;
}

// Checks whether the given directory is a mount point and returns the attachment
// directory of the filesystem mounted at that directory, if it is. Otherwise
// returns NULL.
static InodeRef _Nullable FileHierarchy_AcquireDirectoryMountedAtDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull dir)
{
    decl_try_err();
    const InodeId parentInId = Inode_GetId(dir);
    const FilesystemId parentFsId = Inode_GetFilesystemId(dir);

    List_ForEach(&self->fsDownlinks, FSDownlink,
        if (pCurNode->fsid == parentFsId && pCurNode->inid == parentInId) {
            return Filesystem_ReacquireNode(pCurNode->childFS, pCurNode->childDirectory);
        }
    )

    return NULL;
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

// Returns a path from 'rootDir' to 'dir' in 'buffer'.
errno_t FileHierarchy_GetDirectoryPath(FileHierarchyRef _Nonnull self, InodeRef _Nonnull dir, InodeRef _Nonnull rootDir, User user, char* _Nonnull  pBuffer, size_t bufferSize)
{
    decl_try_err();
    MutablePathComponent pc;

    try_bang(SELock_LockShared(&self->lock));
    InodeRef pCurDir = Inode_Reacquire(dir);

    if (bufferSize < 1) {
        throw(EINVAL);
    }

    // We walk up the filesystem from 'pNode' to the global root of the filesystem
    // and we build the path right aligned in the user provided buffer. We'll move
    // the path such that it'll start at 'pBuffer' once we're done.
    char* p = &pBuffer[bufferSize - 1];
    *p = '\0';

    while (!Inode_Equals(pCurDir, rootDir)) {
        const InodeId childInodeIdToLookup = Inode_GetId(pCurDir);
        InodeRef pParentDir = NULL;

        try(FileHierarchy_AcquireParentDirectory(self, pCurDir, rootDir, user, &pParentDir));
        Inode_Relinquish(pCurDir);
        pCurDir = pParentDir; pParentDir = NULL;

        pc.name = pBuffer;
        pc.count = 0;
        pc.capacity = p - pBuffer;
        try(get_name_of_node(childInodeIdToLookup, pCurDir, user, &pc));

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
    try_bang(SELock_Unlock(&self->lock));
    return EOK;

catch:
    Inode_Relinquish(pCurDir);
    pBuffer[0] = '\0';
    try_bang(SELock_Unlock(&self->lock));
    return err;
}

// Acquires the parent directory of the directory 'pDir'. Returns 'pDir' again
// if that inode is the path resolver's root directory. Returns a suitable error
// code and leaves the iterator unchanged if an error (eg access denied) occurs.
// Walking up means resolving a path component of the form '..'.
static errno_t FileHierarchy_AcquireParentDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeRef _Nonnull rootDir, User user, InodeRef _Nullable * _Nonnull pOutParentDir)
{
    decl_try_err();
    InodeRef pParentDir = NULL;
    InodeRef pMountingDir = NULL;
    InodeRef pParentOfMountingDir = NULL;

    *pOutParentDir = NULL;

    // Do not walk past the root directory
    if (Inode_Equals(pDir, rootDir)) {
        *pOutParentDir = Inode_Reacquire(pDir);
        return EOK;
    }


    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pDir), pDir, &kPathComponent_Parent, user, NULL, &pParentDir));

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
        try(FileHierarchy_AcquireDirectoryMountingDirectory(self, pDir, &pMountingDir));

        Inode_Lock(pMountingDir);
        err = Filesystem_AcquireNodeForName(Inode_GetFilesystem(pMountingDir), pMountingDir, &kPathComponent_Parent, user, NULL, &pParentOfMountingDir);
        Inode_UnlockRelinquish(pMountingDir);

        *pOutParentDir = pParentOfMountingDir;
    }

catch:
    return err;
}

// Acquires the child node 'pName' of the directory 'pDir' and returns EOK if this
// works out. Otherwise returns a suitable error and returns NULL as the node.
// This function handles the case that we want to walk down the filesystem tree
// (meaning that the given path component is a file or directory name and
// neither '.' nor '..').
static errno_t FileHierarchy_AcquireChildNode(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, User user, InodeRef _Nullable * _Nonnull pOutChildNode)
{
    decl_try_err();
    InodeRef pChildNode = NULL;

    *pOutChildNode = NULL;

    // Ask the filesystem for the inode that is named by the tuple (pDir, pName)
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pDir), pDir, pName, user, NULL, &pChildNode));


    // This can only happen if the filesystem is in a corrupted state.
    if (Inode_Equals(pDir, pChildNode)) {
        Inode_Relinquish(pChildNode);
        throw(EIO);
    }


    // Check whether the new inode is a mountpoint. If not then we just return
    // the acquired node as is. Otherwise we'll have to look up the root directory
    // of the mounted filesystem.
    InodeRef pMountedFsRootDir = FileHierarchy_AcquireDirectoryMountedAtDirectory(self, pChildNode);

    if (pMountedFsRootDir == NULL) {
        *pOutChildNode = pChildNode;
    }
    else {
        *pOutChildNode = pMountedFsRootDir;
        Inode_Relinquish(pChildNode);
    }

catch:
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

// Looks up the inode named by the given path. The path may be relative or absolute.
errno_t FileHierarchy_AcquireNodeForPath(FileHierarchyRef _Nonnull self, PathResolution mode, const char* _Nonnull pPath, InodeRef _Nonnull rootDir, InodeRef _Nonnull cwDir, User user, ResolvedPath* _Nonnull pResult)
{
    decl_try_err();
    InodeRef pCurNode = NULL;
    InodeRef pNextNode = NULL;
    PathComponent pc;
    size_t pi = 0;
    bool isLastPathComponent = false;

    ResolvedPath_Init(pResult);

    if (pPath[0] == '\0') {
        return ENOENT;
    }

    try_bang(SELock_LockShared(&self->lock));

    // Start with the root directory if the path starts with a '/' and the
    // current working directory otherwise
    InodeRef pStartNode = (pPath[0] == '/') ? rootDir : cwDir;
    pCurNode = Inode_Reacquire(pStartNode);


    // Iterate through the path components, looking up the inode that corresponds
    // to the current path component. Stop once we hit the end of the path.
    // Note that:
    // * lookup of '.' can not fail with ENOENT because it's the same as the current directory
    // * lookup of '..' can not fail with ENOENT because every directory has a parent (parent of root is root itself)
    // * lookup of a named entry can fail with ENOENT
    Inode_Lock(pCurNode);
    for (;;) {
        try(get_next_path_component(pPath, &pi, &pc, &isLastPathComponent));

        if (pc.count == 0) {
            break;
        }


        // The current directory better be an actual directory
        if (!Inode_IsDirectory(pCurNode)) {
            throw(ENOTDIR);
        }


        if (mode == kPathResolution_PredecessorOfTarget && isLastPathComponent) {
            break;
        }

        if (pc.count == 1 && pc.name[0] == '.') {
            // pCurNode does not change
            continue;
        }
        else if (pc.count == 2 && pc.name[0] == '.' && pc.name[1] == '.') {
            try(FileHierarchy_AcquireParentDirectory(self, pCurNode, rootDir, user, &pNextNode));
        }
        else {
            try(FileHierarchy_AcquireChildNode(self, pCurNode, &pc, user, &pNextNode));
        }

        Inode_UnlockRelinquish(pCurNode);
        pCurNode = pNextNode;
        pNextNode = NULL;

        Inode_Lock(pCurNode);
    }
    Inode_Unlock(pCurNode);
    try_bang(SELock_Unlock(&self->lock));


    // Note that we move ownership of the target node to the result structure
    pResult->inode = pCurNode;
    pResult->lastPathComponent = pc;
    return EOK;

catch:
    Inode_UnlockRelinquish(pCurNode);
    try_bang(SELock_Unlock(&self->lock));
    return err;
}


class_func_defs(FileHierarchy, Object,
override_func_def(deinit, FileHierarchy, Object)
);
