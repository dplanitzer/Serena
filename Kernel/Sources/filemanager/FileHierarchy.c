//
//  FileHierarchy.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/28/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileHierarchy.h"
#include "FilesystemManager.h"
#include <klib/Hash.h>
#include <filesystem/FSUtilities.h>
#include <filesystem/kernfs/KernFS.h>
#include <kern/string.h>
#include <sched/rwmtx.h>
#include <security/SecurityManager.h>

extern void ResolvedPath_Init(ResolvedPath* _Nonnull self);


//#define LOG
#ifdef LOG
#include <log/Log.h>
#endif


// Represents a filesystem and lists all the directories in this filesystem that
// serve as attachments points for other filesystems.
typedef struct FsNode {
    FilesystemRef _Nonnull  filesystem;
    List                    attachmentPoints;   // List<AtNode>
} FsNode;

// Represents a single attachment point in a filesystem. Names the directory and
// its filesystem that are attached at a directory in the parent filesystem.
typedef struct AtNode {
    ListNode                sibling;

    InodeRef _Nonnull       attachingDirectory;     // Owning
    FsNode* _Nonnull _Weak  attachingFsNode;

    ino_t                   attachedDirectoryId;    // Owning
    FsNode* _Nonnull        attachedFsNode;         // Owning
} AtNode;

typedef enum FHKeyType {
    kKeyType_Uplink,        // Represents moving from the root directory of an attached filesystem to the attaching directory inside the filesystem we're attached to
    kKeyType_Downlink       // Represents moving from the attaching filesystem down to the attached filesystem. Use this to check whether the file hierarchy knows a particular fsid, inid
} FHKeyType;

// Hashtable entry that maps the (fsid, inid, type) of a directory to the
// corresponding attachment point (AtNode). There is an entry for the attachment
// and the attaching directory. Both point to the same AtNode.
typedef struct FHKey {
    ListNode            sibling;

    AtNode* _Nonnull    at;

    fsid_t              fsid;
    ino_t               inid;
    FHKeyType           type;
} FHKey;


#define HASH_CHAINS_COUNT   8
#define HASH_CHAINS_MASK    (HASH_CHAINS_COUNT - 1)

#define HASH_INDEX(__direction, __fsid, __inid) \
    (hash_scalar(((size_t)__direction) + ((size_t)__fsid) + ((size_t)__inid)) & HASH_CHAINS_MASK)

final_class_ivars(FileHierarchy, Object,
    rwmtx_t             lock;
    FsNode* _Nonnull    root;
    InodeRef _Nonnull   rootDirectory;
    List                hashChain[HASH_CHAINS_COUNT];   // Chains of FHKey
);


static void print_atnode(AtNode* _Nonnull self);
static void destroy_atnode(AtNode* _Nullable self);
static void _FileHierarchy_DestroyAllKeys(FileHierarchyRef _Nonnull self);
static errno_t FileHierarchy_AcquireParentDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeRef _Nonnull rootDir, uid_t uid, gid_t gid, ino_t* _Nullable pInOutMountingDirId, InodeRef _Nullable * _Nonnull pOutParentDir);


static void destroy_fsnode(FsNode* _Nullable self)
{
    if (self) {
        List_ForEach(&self->attachmentPoints, AtNode,
            destroy_atnode(pCurNode);
        );

        Object_Release(self->filesystem);
        self->filesystem = NULL;
        FSDeallocate(self);
    }
}

static errno_t create_fsnode(FilesystemRef _Nonnull fs, FsNode* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FsNode* self = NULL;

    if ((err = FSAllocateCleared(sizeof(FsNode), (void**)&self)) == EOK) {
        self->filesystem = Object_RetainAs(fs, Filesystem);
    }
    *pOutSelf = self;
    return err;
}

#if LOG
static void print_fsnode(FsNode* _Nonnull self)
{
    printf("FsNode {\n");
    printf("fsid: %u, fs: %p\n", Filesystem_GetId(self->filesystem), (void*)self->filesystem);
    printf("attachment points:\n");
    List_ForEach(&self->attachmentPoints, AtNode,
        print_atnode(pCurNode);
    );
    printf("}\n");
}
#endif

static void destroy_atnode(AtNode* _Nullable self)
{
    if (self) {
        Inode_Relinquish(self->attachingDirectory);
        destroy_fsnode(self->attachedFsNode);
        FSDeallocate(self);
    }
}

static errno_t create_atnode(FsNode* _Nonnull atFsNode, InodeRef _Nonnull atDir, FilesystemRef _Nonnull fs, AtNode* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AtNode* self = NULL;
    FsNode* fsNode = NULL;
    InodeRef rootDir = NULL;

    try(create_fsnode(fs, &fsNode));
    try(Filesystem_AcquireRootDirectory(fs, &rootDir));
    try(FSAllocateCleared(sizeof(AtNode), (void**)&self));
    self->attachingDirectory = Inode_Reacquire(atDir);
    self->attachingFsNode = atFsNode;
    self->attachedDirectoryId = Inode_GetId(rootDir);
    self->attachedFsNode = fsNode;
    Inode_Relinquish(rootDir);
    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    Filesystem_RelinquishNode(fs, rootDir);
    destroy_fsnode(fsNode);
    return err;
}

#if LOG
static void print_atnode(AtNode* _Nonnull self)
{
    printf("AtNode {\n");
    printf("  attaching-dir id: %u, i-node: %p\n", Inode_GetId(self->attachingDirectory), (void*)self->attachingDirectory);
    printf("  attached-dir  id: %u\n", self->attachedDirectoryId);
    printf("}\n");
}
#endif

static errno_t create_key(fsid_t fsid, ino_t inid, FHKeyType type, AtNode* _Nonnull node, FHKey* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FHKey* self = NULL;

    if ((err = FSAllocate(sizeof(FHKey), (void**)&self)) == EOK) {
        self->at = node;
        self->fsid = fsid;
        self->inid = inid;
        self->type = type;
    }
    *pOutSelf = self;
    return err;
}

static void destroy_key(FHKey* _Nullable self)
{
    if (self) {
        self->at = NULL;
        FSDeallocate(self);
    }
}


errno_t FileHierarchy_Create(FilesystemRef _Nonnull rootFS, FileHierarchyRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FileHierarchyRef self;

    try(Object_Create(class(FileHierarchy), 0, (void**)&self));
    rwmtx_init(&self->lock);
    
    for (int i = 0; i < HASH_CHAINS_COUNT; i++) {
        List_Init(&self->hashChain[i]);
    }

    try(Filesystem_AcquireRootDirectory(rootFS, &self->rootDirectory));
    try(create_fsnode(rootFS, &self->root));

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    Object_Release(self);
    return err;
}

void FileHierarchy_deinit(FileHierarchyRef _Nullable self)
{
    _FileHierarchy_DestroyAllKeys(self);
    if (self->rootDirectory) {
        Inode_Relinquish(self->rootDirectory);
        self->rootDirectory = NULL;
    }
    if (self->root) {
        destroy_fsnode(self->root);
        self->root = NULL;
    }

    rwmtx_deinit(&self->lock);
}


static void _FileHierarchy_DestroyAllKeys(FileHierarchyRef _Nonnull self)
{
    for (size_t i = 0; i < HASH_CHAINS_COUNT; i++) {
        List_ForEach(&self->hashChain[i], FHKey,
            destroy_key(pCurNode);
        )
    }
}



// Returns a strong reference to the root filesystem of the given file hierarchy.
FilesystemRef FileHierarchy_CopyRootFilesystem(FileHierarchyRef _Nonnull self)
{
    return Object_RetainAs(self->root->filesystem, Filesystem);
}

// Returns the root directory of the given file hierarchy.
InodeRef _Nonnull FileHierarchy_AcquireRootDirectory(FileHierarchyRef _Nonnull self)
{
    return Filesystem_ReacquireNode(self->root->filesystem, self->rootDirectory);
}

static void _FileHierarchy_InsertKey(FileHierarchyRef _Nonnull _Locked self, FHKey* _Nonnull key)
{
    const size_t hashIdx = HASH_INDEX(key->type, key->fsid, key->inid);

    List_InsertBeforeFirst(&self->hashChain[hashIdx], &key->sibling);
}

static void _FileHierarchy_RemoveKey(FileHierarchyRef _Nonnull _Locked self, FHKey* _Nullable key)
{
    if (key) {
        const size_t hashIdx = HASH_INDEX(key->type, key->fsid, key->inid);

        List_Remove(&self->hashChain[hashIdx], &key->sibling);
    }
}

static FHKey* _Nullable _FileHierarchy_GetKey(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull inode, FHKeyType type)
{
    const fsid_t fsid = Inode_GetFilesystemId(inode);
    const ino_t inid = Inode_GetId(inode);
    const size_t hashIdx = HASH_INDEX(type, fsid, inid);

    List_ForEach(&self->hashChain[hashIdx], FHKey,
        if (pCurNode->type == type && pCurNode->fsid == fsid && pCurNode->inid == inid) {
            return pCurNode;
        }
    );

    return NULL;
}

static AtNode* _Nullable _FileHierarchy_GetAtNode(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull inode, FHKeyType type)
{
    FHKey* key = _FileHierarchy_GetKey(self, inode, type);

    return (key) ? key->at : NULL;
}

static FsNode* _Nullable find_fsnode_rec(FsNode* _Nonnull self, fsid_t fsid)
{
    if (Filesystem_GetId(self->filesystem) == fsid) {
        return self;
    }

    List_ForEach(&self->attachmentPoints, AtNode, 
        FsNode* foundNode = find_fsnode_rec(pCurNode->attachedFsNode, fsid);

        if (foundNode) {
            return foundNode;
        }
    );

    return NULL;
}

static FsNode* _Nullable _FileHierarchy_GetFsNode(FileHierarchyRef _Nonnull _Locked self, fsid_t fsid)
{
    return find_fsnode_rec(self->root, fsid);
}

// Attaches the root directory of the filesystem 'fs' to the directory 'atDir'. 'atDir'
// must be a member of this file hierarchy and may not already have another filesystem
// attached to it.
errno_t FileHierarchy_AttachFilesystem(FileHierarchyRef _Nonnull self, FilesystemRef _Nonnull fs, InodeRef _Nonnull atDir)
{
    decl_try_err();
    FsNode* atFsNode = NULL;
    AtNode* atNode = NULL;
    FHKey* upKey = NULL;
    FHKey* downKey = NULL;
    InodeRef fsRootDir = NULL;

    if (!S_ISDIR(Inode_GetMode(atDir))) {
        return ENOTDIR;
    }

    try_bang(rwmtx_wrlock(&self->lock));

    // Make sure that the filesystem that owns 'atDir' exists in our file hierarchy
    atFsNode = _FileHierarchy_GetFsNode(self, Inode_GetFilesystemId(atDir));
    if (atFsNode == NULL) {
        throw(EINVAL);
    }

    // Make sure that 'atDir' doesn't already serve as a mount point in our file hierarchy
    if (_FileHierarchy_GetAtNode(self, atDir, kKeyType_Downlink) != NULL) {
        throw(EBUSY);
    }

    try(create_atnode(atFsNode, atDir, fs, &atNode));
    try(create_key(Filesystem_GetId(fs), atNode->attachedDirectoryId, kKeyType_Uplink, atNode, &upKey));
    try(create_key(Inode_GetFilesystemId(atDir), Inode_GetId(atDir), kKeyType_Downlink, atNode, &downKey));

    _FileHierarchy_InsertKey(self, upKey);
    _FileHierarchy_InsertKey(self, downKey);

    List_InsertAfterLast(&atFsNode->attachmentPoints, &atNode->sibling);

    try_bang(rwmtx_unlock(&self->lock));
    return EOK;

catch:
    destroy_key(upKey);
    destroy_key(downKey);
    destroy_atnode(atNode);

    try_bang(rwmtx_unlock(&self->lock));

    return err;
}

static void _FileHierarchy_CollectKeysForAtNode(FileHierarchyRef _Nonnull _Locked self, AtNode* _Nonnull atNode, List* _Nonnull keys)
{
    for (size_t i = 0; i < HASH_CHAINS_COUNT; i++) {
        List_ForEach(&self->hashChain[i], FHKey,
            if (pCurNode->at == atNode) {
                List_Remove(&self->hashChain[i], &pCurNode->sibling);
                List_InsertBeforeFirst(keys, &pCurNode->sibling);
            }
        );
    }
}

static void destroy_key_collection(List* _Nonnull keys)
{
    FHKey* curKey = (FHKey*)keys->first;

    while (curKey) {
        FHKey* nextKey = (FHKey*)curKey->sibling.next;

        destroy_key(curKey);
        curKey = nextKey;
    }
}

// Detaches the filesystem who's directory 'dir' is attached to another filesystem.
// The detachment will fail if the filesystem which is attached to 'dir' hosts
// other attached filesystems. However, the detachment will be recursively
// applied if 'forced' is true.
errno_t FileHierarchy_DetachFilesystemAt(FileHierarchyRef _Nonnull self, InodeRef _Consuming _Nonnull dir, bool forced)
{
    decl_try_err();
    AtNode* atNode = NULL;
    FsNode* atFsNode = NULL;
    FilesystemRef fs = NULL;
    List keys;

    List_Init(&keys);
    try_bang(rwmtx_wrlock(&self->lock));

    // Make sure that the FS that we want to detach, isn't the root FS
    if (Inode_Equals(self->rootDirectory, dir)) {
        throw(EBUSY);
    }


    // 'dir' is the directory that is attached to the mountpoint. Thus we want
    // to get the uplink here.
    FHKey* upKey = _FileHierarchy_GetKey(self, dir, kKeyType_Uplink);
    if (upKey == NULL) {
        throw(EINVAL);
    }   


    // Make sure that the FS that we want to detach doesn't have other FS'
    // attached to it.
    atNode = upKey->at;
    fs = atNode->attachedFsNode->filesystem;
    if (!forced && !List_IsEmpty(&atNode->attachedFsNode->attachmentPoints)) {
        throw(EBUSY);
    }


    // Drop what should be the last inode reference on the filesystem and then
    // try to stop it. This may fail with an EBUSY if someone else still has
    // inodes acquired. Note that the FS manager never stops catalog filesystems.
    Inode_Relinquish(dir);
#ifndef __DISKIMAGE__
    err = FilesystemManager_StopFilesystem(gFilesystemManager, fs, forced);
#endif

    if (err != EBUSY) {
        atFsNode = atNode->attachingFsNode;
        List_Remove(&atFsNode->attachmentPoints, &atNode->sibling);
        _FileHierarchy_CollectKeysForAtNode(self, atNode, &keys);
    }

    try_bang(rwmtx_unlock(&self->lock));

    // Now that we've dropped the lock, do the time consuming stuff (disbanding
    // an FS may push data to the disk and block for a while).
    if (err != EBUSY) {
#ifndef __DISKIMAGE__
        FilesystemManager_DisbandFilesystem(gFilesystemManager, fs);
#endif
        destroy_key_collection(&keys);
        destroy_atnode(atNode);
    }

    return EOK;

catch:
    try_bang(rwmtx_unlock(&self->lock));
    return err;
}

// Returns true if the given (directory) inode is an attachment point for another
// filesystem.
bool FileHierarchy_IsAttachmentPoint(FileHierarchyRef _Nonnull self, InodeRef _Nonnull inode)
{
    rwmtx_rdlock(&self->lock);
    const bool r = (_FileHierarchy_GetAtNode(self, inode, kKeyType_Downlink) != NULL) ? true : false;
    rwmtx_unlock(&self->lock);
    return r;
}


// Acquires the inode that is mounting the given directory. A suitable error
// and NULL is returned if the given directory is not mounted (anymore) or some
// other problem is detected.
// EOK and NULL are returned if 'dir' is the root directory of the hierarchy.
static errno_t _FileHierarchy_AcquireDirectoryMountingDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull dir, InodeRef _Nullable _Locked * _Nonnull pOutMountingDir)
{
    AtNode* node = _FileHierarchy_GetAtNode(self, dir, kKeyType_Uplink);

    if (node) {
        *pOutMountingDir = Inode_Reacquire(node->attachingDirectory);
        return EOK;
    }
    else if (Filesystem_GetId(self->root->filesystem) == Inode_GetFilesystemId(dir) && Inode_GetId(self->rootDirectory) == Inode_GetId(dir)) {
        *pOutMountingDir = NULL;
        return EOK;
    }
    else {
        *pOutMountingDir = NULL;
        return ENOENT;
    }
}

// Checks whether the given directory is a mount point and returns the attachment
// directory of the filesystem mounted at that directory, if it is. Otherwise
// returns NULL.
static InodeRef _Nullable _FileHierarchy_AcquireDirectoryMountedAtDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull dir)
{
    AtNode* node = _FileHierarchy_GetAtNode(self, dir, kKeyType_Downlink);
    InodeRef ip = NULL;

    if (node) {
        Filesystem_AcquireRootDirectory(node->attachedFsNode->filesystem, &ip);
    }
    return ip;
}



// Atomically looks up the name of the node 'idOfNodeToLookup' in the directory
// 'pDir' and returns it in 'pc' if successful. This lookup may fail with ENOENT
// which happens if the node has been removed from the directory. It may fail
// with EACCESS if the directory lacks search and read permissions for the user
// 'uid'.
static errno_t get_name_of_node(ino_t idOfNodeToLookup, InodeRef _Nonnull pDir, uid_t uid, gid_t gid, MutablePathComponent* _Nonnull pc)
{
    decl_try_err();

    Inode_Lock(pDir);
    err = SecurityManager_CheckNodeAccess(gSecurityManager, pDir, uid, gid, R_OK | X_OK);
    if (err == EOK) {
        err = Filesystem_GetNameOfNode(Inode_GetFilesystem(pDir), pDir, idOfNodeToLookup, pc);
    }
    Inode_Unlock(pDir);
    return err;
}

// Returns a path from 'rootDir' to 'node' in 'buffer'. This function guarantees
// that it will always be able to produce a path if 'node' is a directory (assuming
// that the caller has the necessary permissions). However, some filesystem
// implementations are able to produce a path even if 'node' is a file or some
// other kind of inode. ENOTSUP is returned if a path can not be produced because
// the filesystem doesn't support doing that for the type of node that 'node' is.
errno_t FileHierarchy_GetPath(FileHierarchyRef _Nonnull self, InodeRef _Nonnull node, InodeRef _Nonnull rootDir, uid_t uid, gid_t gid, char* _Nonnull  pBuffer, size_t bufferSize)
{
    decl_try_err();
    MutablePathComponent pc;

    try_bang(rwmtx_rdlock(&self->lock));
    InodeRef pCurDir = Inode_Reacquire(node);

    if (bufferSize < 1) {
        throw(EINVAL);
    }

    // We walk up the filesystem from 'pNode' to the global root of the filesystem
    // and we build the path right aligned in the user provided buffer. We'll move
    // the path such that it'll start at 'pBuffer' once we're done.
    char* p = &pBuffer[bufferSize - 1];
    *p = '\0';

    while (!Inode_Equals(pCurDir, rootDir)) {
        ino_t childInodeIdToLookup = Inode_GetId(pCurDir);
        InodeRef pParentDir = NULL;

        try(FileHierarchy_AcquireParentDirectory(self, pCurDir, rootDir, uid, gid, &childInodeIdToLookup, &pParentDir));
        Inode_Relinquish(pCurDir);
        pCurDir = pParentDir; pParentDir = NULL;

        pc.name = pBuffer;
        pc.count = 0;
        pc.capacity = p - pBuffer;
        try(get_name_of_node(childInodeIdToLookup, pCurDir, uid, gid, &pc));

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
    try_bang(rwmtx_unlock(&self->lock));
    return EOK;

catch:
    Inode_Relinquish(pCurDir);
    pBuffer[0] = '\0';
    try_bang(rwmtx_unlock(&self->lock));
    return err;
}

// Acquires the parent directory of the directory 'pDir'. Returns 'pDir' again
// if that inode is the path resolver's root directory. Returns a suitable error
// code and leaves the iterator unchanged if an error (eg access denied) occurs.
// Walking up means resolving a path component of the form '..'.
static errno_t FileHierarchy_AcquireParentDirectory(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, InodeRef _Nonnull rootDir, uid_t uid, gid_t gid, ino_t* _Nullable pInOutMountingDirId, InodeRef _Nullable * _Nonnull pOutParentDir)
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


    try(SecurityManager_CheckNodeAccess(gSecurityManager, pDir, uid, gid, X_OK));
    try(Filesystem_AcquireParentNode(Inode_GetFilesystem(pDir), pDir, &pParentDir));

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
        try(_FileHierarchy_AcquireDirectoryMountingDirectory(self, pDir, &pMountingDir));

        if (pInOutMountingDirId) {
            *pInOutMountingDirId = Inode_GetId(pMountingDir);
        }

        Inode_Lock(pMountingDir);
        err = SecurityManager_CheckNodeAccess(gSecurityManager, pMountingDir, uid, gid, X_OK);
        if (err == EOK) {
            err = Filesystem_AcquireParentNode(Inode_GetFilesystem(pMountingDir), pMountingDir, &pParentOfMountingDir);
        }
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
static errno_t FileHierarchy_AcquireChildNode(FileHierarchyRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, uid_t uid, gid_t gid, InodeRef _Nullable * _Nonnull pOutChildNode)
{
    decl_try_err();
    InodeRef pChildNode = NULL;

    *pOutChildNode = NULL;

    // Ask the filesystem for the inode that is named by the tuple (pDir, pName)
    try(SecurityManager_CheckNodeAccess(gSecurityManager, pDir, uid, gid, X_OK));
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pDir), pDir, pName, NULL, &pChildNode));


    // This can only happen if the filesystem is in a corrupted state.
    if (Inode_Equals(pDir, pChildNode)) {
        Inode_Relinquish(pChildNode);
        throw(EIO);
    }


    // Check whether the new inode is a mountpoint. If not then we just return
    // the acquired node as is. Otherwise we'll have to look up the root directory
    // of the mounted filesystem.
    InodeRef pMountedFsRootDir = _FileHierarchy_AcquireDirectoryMountedAtDirectory(self, pChildNode);

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
errno_t FileHierarchy_AcquireNodeForPath(FileHierarchyRef _Nonnull self, PathResolution mode, const char* _Nonnull pPath, InodeRef _Nonnull rootDir, InodeRef _Nonnull cwDir, uid_t uid, gid_t gid, ResolvedPath* _Nonnull pResult)
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

    try_bang(rwmtx_rdlock(&self->lock));

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
        if (!S_ISDIR(Inode_GetMode(pCurNode))) {
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
            try(FileHierarchy_AcquireParentDirectory(self, pCurNode, rootDir, uid, gid, NULL, &pNextNode));
        }
        else {
            try(FileHierarchy_AcquireChildNode(self, pCurNode, &pc, uid, gid, &pNextNode));
        }

        Inode_UnlockRelinquish(pCurNode);
        pCurNode = pNextNode;
        pNextNode = NULL;

        Inode_Lock(pCurNode);
    }
    Inode_Unlock(pCurNode);
    try_bang(rwmtx_unlock(&self->lock));


    // Note that we move ownership of the target node to the result structure
    pResult->inode = pCurNode;
    pResult->lastPathComponent = pc;
    return EOK;

catch:
    Inode_UnlockRelinquish(pCurNode);
    try_bang(rwmtx_unlock(&self->lock));
    return err;
}


class_func_defs(FileHierarchy, Object,
override_func_def(deinit, FileHierarchy, Object)
);
