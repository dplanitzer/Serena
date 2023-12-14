//
//  RamFS.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "RamFSPriv.h"
#include "FilesystemManager.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Directory Inode
////////////////////////////////////////////////////////////////////////////////

static ErrorCode DirectoryNode_Create(RamFSRef _Nonnull self, InodeId id, RamFS_DirectoryRef _Nullable pParentDir, FilePermissions permissions, User user, RamFS_DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    RamFS_DirectoryRef pDir;

    try(Inode_AbstractCreate(&kRamFS_DirectoryClass, kInode_Directory, id, Filesystem_GetId(self), permissions, user, 0ll, (InodeRef*)&pDir));
    try(GenericArray_Init(&pDir->header, sizeof(RamFS_DirectoryEntry), 8));
    try(DirectoryNode_AddWellKnownEntry(pDir, ".", (InodeRef) pDir));
    try(DirectoryNode_AddWellKnownEntry(pDir, "..", (pParentDir) ? (InodeRef) pParentDir : (InodeRef) pDir));

    *pOutDir = pDir;
    return EOK;

catch:
    Object_Release(pDir);
    *pOutDir = NULL;
    return err;
}

static ErrorCode _Nullable DirectoryNode_CopyParent(RamFS_DirectoryRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutParent)
{
    RamFS_DirectoryHeader* pHeader = &self->header;
    const RamFS_DirectoryEntry* pParentEntry = GenericArray_GetRefAt(pHeader, RamFS_DirectoryEntry, 1);

    if (pParentEntry->node != (InodeRef) self) {
        *pOutParent = Object_RetainAs(pParentEntry->node, Inode);
        return EOK;
    } else {
        *pOutParent = NULL;
        return ENOENT;
    }
}

static ErrorCode DirectoryNode_CopyNodeForName(RamFS_DirectoryRef _Nonnull self, const PathComponent* pComponent, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    RamFS_DirectoryHeader* pHeader = &self->header;
    const RamFS_DirectoryEntry* pEntry = NULL;

    if (pComponent->count > kMaxFilenameLength) {
        throw(ENAMETOOLONG);
    }

    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
        const RamFS_DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, RamFS_DirectoryEntry, i);

        if (String_EqualsUpTo(pCurEntry->filename, pComponent->name, __min(pComponent->count, kMaxFilenameLength))) {
            pEntry = pCurEntry;
            break;
        }
    }

    if (pEntry) {
        *pOutNode = Object_RetainAs(pEntry->node, Inode);
        return EOK;
    } else {
        throw(ENOENT);
    }

catch:
    *pOutNode = NULL;
    return err;
}

static ErrorCode DirectoryNode_GetNameOfNode(RamFS_DirectoryRef _Nonnull self, InodeId id, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();
    RamFS_DirectoryHeader* pHeader = &self->header;
    const RamFS_DirectoryEntry* pEntry = NULL;

    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
        const RamFS_DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, RamFS_DirectoryEntry, i);

        if (Inode_GetId(pCurEntry->node) == id) {
            pEntry = pCurEntry;
            break;
        }
    }

    if (pEntry) {
        const ByteCount len = String_LengthUpTo(pEntry->filename, kMaxFilenameLength);

        if (len > pComponent->capacity) {
            throw(ERANGE);
        }

        String_CopyUpTo(pComponent->name, pEntry->filename, len);
        pComponent->count = len;
        return EOK;
    } else {
        throw(ENOENT);
    }

catch:
    pComponent->count = 0;
    return err;
}

// Adds a weak reference to the given Inode as a well-known entry (".", "..") to the directory
static ErrorCode DirectoryNode_AddWellKnownEntry(RamFS_DirectoryRef _Nonnull self, const Character* _Nonnull pName, InodeRef _Nonnull pNode)
{
    RamFS_DirectoryEntry entry;
    Character* p = String_CopyUpTo(entry.filename, pName, kMaxFilenameLength);
    while (p < &entry.filename[kMaxFilenameLength]) *p++ = '\0';
    entry.node = pNode;

    ErrorCode err = EOK;
    GenericArray_InsertAt(err, &self->header, entry, RamFS_DirectoryEntry, GenericArray_GetCount(&self->header));
    if (err == EOK) {
        Inode_IncrementFileSize(self, sizeof(entry));
    }

    return err;
}

static ErrorCode DirectoryNode_AddEntry(RamFS_DirectoryRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull pChildNode)
{
    if (pName->count > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    RamFS_DirectoryEntry entry;
    Character* p = String_CopyUpTo(entry.filename, pName->name, pName->count);
    while (p < &entry.filename[kMaxFilenameLength]) *p++ = '\0';
    entry.node = Object_RetainAs(pChildNode, Inode);

    ErrorCode err = EOK;
    GenericArray_InsertAt(err, &self->header, entry, RamFS_DirectoryEntry, GenericArray_GetCount(&self->header));
    if (err == EOK) {
        Inode_IncrementFileSize(self, sizeof(entry));
    }

    return err;
}

static Int Directory_ReadEntries(RamFS_DirectoryRef _Nonnull self, DirectoryEntry* pBuffer, Int startIndex, Int nMaxEntriesToRead)
{
    RamFS_DirectoryHeader* pHeader = &self->header;
    const Int readLimit = __min(GenericArray_GetCount(pHeader), startIndex + nMaxEntriesToRead);
    Int nEntriesRead = 0;

    for (Int i = startIndex; i < readLimit; i++) {
        const RamFS_DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, RamFS_DirectoryEntry, i);

        pBuffer->inodeId = Inode_GetId(pCurEntry->node);
        String_CopyUpTo(pBuffer->name, pCurEntry->filename, kMaxFilenameLength);

        pBuffer++;
        nEntriesRead++;
    }

    return nEntriesRead;
}

void RamFS_Directory_deinit(RamFS_DirectoryRef _Nonnull self)
{
    RamFS_DirectoryHeader* pHeader = &self->header;

    // Note that the first 2 entries are "." and ".." respective and that they
    // are weak references.
    for (Int i = 2; i < GenericArray_GetCount(pHeader); i++) {
        RamFS_DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, RamFS_DirectoryEntry, i);

        Object_Release(pCurEntry->node);
        pCurEntry->node = NULL;
    }
    GenericArray_Deinit(pHeader);
}

CLASS_METHODS(RamFS_Directory, Inode,
OVERRIDE_METHOD_IMPL(deinit, RamFS_Directory, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: RAM Disk
////////////////////////////////////////////////////////////////////////////////

// Creates an instance of RAM-FS. RAM-FS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
ErrorCode RamFS_Create(User rootDirUser, RamFSRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    RamFSRef self;

    try(Filesystem_Create(&kRamFSClass, (FilesystemRef*)&self));
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->notifier);
    self->root = NULL;
    self->rootDirUser = rootDirUser;
    self->nextAvailableInodeId = 0;
    self->busyCount = 0;
    self->isReadOnly = false;

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void RamFS_deinit(RamFSRef _Nonnull self)
{
    Object_Release(self->root);
    self->root = NULL;
    ConditionVariable_Deinit(&self->notifier);
    Lock_Deinit(&self->lock);
}

static InodeId RamFS_GetNextAvailableInodeId_Locked(RamFSRef _Nonnull self)
{
    return (InodeId) self->nextAvailableInodeId++;
}

// Call this function at the beginning of a FS operation. It validates that starting
// the operation is permissible.
static ErrorCode RamFS_BeginOperation(RamFSRef _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->lock);

    // This function should only ever be called while the FS is mounted
    assert(self->root != NULL);

    self->busyCount++;

catch:
    Lock_Unlock(&self->lock);
    return err;
}

// Call this function at the end of a FS operation.
static void RamFS_EndOperation(RamFSRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    self->busyCount--;

    if (self->busyCount == 0) {
        ConditionVariable_BroadcastAndUnlock(&self->notifier, &self->lock);
    } else {
        Lock_Unlock(&self->lock);
    }
}

// Checks whether the given user should be granted access to the given node based
// on the requested permission. Returns EOK if access should be granted and a suitable
// error code if it should be denied.
static ErrorCode RamFS_CheckAccess_Locked(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, User user, FilePermissions permission)
{
    if (permission == kFilePermission_Write) {
        if (self->isReadOnly) {
            return EROFS;
        }

        // XXX once we support actual text mapping, we'll need to check whether the text file is in use
    }

    return Inode_CheckAccess(pNode, user, permission);
}


// Invoked when an instance of this file system is mounted. Note that the
// kernel guarantees that no operations will be issued to the filesystem
// before onMount() has returned with EOK.
ErrorCode RamFS_onMount(RamFSRef _Nonnull self, const Byte* _Nonnull pParams, ByteCount paramsSize)
{
    decl_try_err();

    Lock_Lock(&self->lock);

    if (self->root) {
        throw(EIO);
    }

    const FilePermissions scopePerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirPerms = FilePermissions_Make(scopePerms, scopePerms, scopePerms);

    try(DirectoryNode_Create(self, RamFS_GetNextAvailableInodeId_Locked(self), NULL, dirPerms, self->rootDirUser, &self->root));

catch:
    Lock_Unlock(&self->lock);
    return err;
}

// Invoked when a mounted instance of this file system is unmounted. A file
// system may return an error. Note however that this error is purely advisory
// and the file system implementation is required to do everything it can to
// successfully unmount. Unmount errors are ignored and the file system manager
// will complete the unmount in any case.
ErrorCode RamFS_onUnmount(RamFSRef _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->root == NULL) {
        throw(EIO);
    }

    // There might be one or more operations currently ongoing. Wait until they
    // are done.
    while(self->busyCount > 0) {
        try(ConditionVariable_Wait(&self->notifier, &self->lock, kTimeInterval_Infinity));
    }


    // Make sure that there are no open files anywhere referencing us
    if (!FilesystemManager_CanSafelyUnmountFilesystem(gFilesystemManager, (FilesystemRef)self)) {
        throw(EBUSY);
    }

    // XXX Flush dirty buffers to disk

    Object_Release(self->root);
    self->root = NULL;

catch:
    Lock_Unlock(&self->lock);
    return err;
}


// Returns the root node of the filesystem if the filesystem is currently in
// mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
ErrorCode RamFS_copyRootNode(RamFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutNode)
{
    Lock_Lock(&self->lock);
    *pOutNode = (InodeRef) self->root;
    Lock_Unlock(&self->lock);

    return (*pOutNode) ? EOK : ENOENT;
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function has the support the special names "." (node itself) and ".."
// (parent of node) in addition to "regular" filenames. If the path component
// name is longer than what is supported by the file system, ENAMETOOLONG
// should be returned.
ErrorCode RamFS_copyNodeForName(RamFSRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pParentNode);
    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Execute));
    try(DirectoryNode_CopyNodeForName((RamFS_DirectoryRef) pParentNode, pComponent, pOutNode));

catch:
    Inode_Unlock(pParentNode);
    RamFS_EndOperation(self);
    
    return err;
}

// Returns the name of the node with the id 'id' which a child of the
// directory node 'pParentNode'. 'id' may be of any type. The name is
// returned in the mutable path component 'pComponent'. 'count' in path
// component is 0 on entry and should be set to the actual length of the
// name on exit. The function is expected to return EOK if the parent node
// contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
// file system is > the capacity of the path component, then ERANGE should
// be returned.
ErrorCode RamFS_getNameOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pParentNode);
    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Read | kFilePermission_Execute));
    try(DirectoryNode_GetNameOfNode((RamFS_DirectoryRef) pParentNode, id, pComponent));

catch:
    Inode_Unlock(pParentNode);
    RamFS_EndOperation(self);

    return err;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
ErrorCode RamFS_getFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pNode);
    Inode_GetFileInfo(pNode, pOutInfo);
    Inode_Unlock(pNode);
    RamFS_EndOperation(self);

    return EOK;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
ErrorCode RamFS_setFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, User user, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pNode);
    try(Inode_SetFileInfo(pNode, user, pInfo));

catch:
    Inode_Unlock(pNode);
    RamFS_EndOperation(self);
    return err;
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions. Returns EEXIST if a node with
// the given name already exists.
ErrorCode RamFS_createDirectory(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull pParentNode, User user, FilePermissions permissions)
{
    decl_try_err();
    RamFS_DirectoryRef pNewDir;

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pParentNode);

    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }

    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));
    InodeRef pDummy;
    if (DirectoryNode_CopyNodeForName((RamFS_DirectoryRef)pParentNode, pName, &pDummy) != ENOENT) {
        Object_Release(pDummy);
        throw(EEXIST);
    }

    try(DirectoryNode_Create(self, RamFS_GetNextAvailableInodeId_Locked(self), (RamFS_DirectoryRef)pParentNode, permissions, user, &pNewDir));
    try(DirectoryNode_AddEntry((RamFS_DirectoryRef)pParentNode, pName, (InodeRef)pNewDir));

catch:
    Object_Release(pNewDir);
    Inode_Unlock(pParentNode);
    RamFS_EndOperation(self);
    return err;
}

// Opens the directory represented by the given node. Returns a directory
// descriptor object which is the I/O channel that allows you to read the
// directory content.
ErrorCode RamFS_openDirectory(RamFSRef _Nonnull self, InodeRef _Nonnull pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pDirNode);
    try(Inode_CheckAccess(pDirNode, user, kFilePermission_Read));
    try(Directory_Create((FilesystemRef)self, pDirNode, pOutDir));

catch:
    Inode_Unlock(pDirNode);
    RamFS_EndOperation(self);
    return err;
}

// Reads the next set of directory entries. The first entry read is the one
// at the current directory index stored in 'pDir'. This function guarantees
// that it will only ever return complete directories entries. It will never
// return a partial entry. Consequently the provided buffer must be big enough
// to hold at least one directory entry. Note that this function is expected
// to return "." for the entry at index #0 and ".." for the entry at index #1.
ByteCount RamFS_readDirectory(RamFSRef _Nonnull self, DirectoryRef _Nonnull pDir, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    decl_try_err();
    RamFS_DirectoryRef pDirNode = (RamFS_DirectoryRef)Directory_GetInode(pDir);

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pDirNode);

    const Int nMaxEntriesToRead = nBytesToRead / sizeof(DirectoryEntry);
    const Int nEntriesRead = Directory_ReadEntries(pDirNode, (DirectoryEntry*)pBuffer, (Int) Directory_GetOffset(pDir), nMaxEntriesToRead);
    
    if (nEntriesRead > 0) {
        Directory_IncrementOffset(pDir, nEntriesRead);
    }

    Inode_Unlock(pDirNode);
    RamFS_EndOperation(self);

    return nEntriesRead * sizeof(DirectoryEntry);
}

// Verifies that the given node is accessible assuming the given access mode.
ErrorCode RamFS_checkAccess(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, User user, Int mode)
{
    decl_try_err();

    if ((err = RamFS_BeginOperation(self)) != EOK) {
        return err;
    }
    Inode_Lock(pNode);

    if ((mode & kAccess_Readable) == kAccess_Readable) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Read);
    }
    if (err == EOK && ((mode & kAccess_Writable) == kAccess_Writable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Write);
    }
    if (err == EOK && ((mode & kAccess_Executable) == kAccess_Executable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Execute);
    }

    Inode_Unlock(pNode);
    RamFS_EndOperation(self);

    return err;
}

// Unlink the node identified by the path component 'pName' and which is an
// immediate child of the (directory) node 'pParentNode'. The parent node is
// guaranteed to be a node owned by the filesystem.
// This function must validate that a directory entry with the given name
// actually exists, is a file or an empty directory before it attempts to
// remove the entry from the parent node.
ErrorCode RamFS_unlink(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull pParentNode, User user)
{
    // XXX implement me
    return EACCESS;
}

// Renames the node with name 'pName' and which is an immediate child of the
// node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
// the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
ErrorCode RamFS_rename(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull pNewParentNode, User user)
{
    // XXX implement me
    return EACCESS;
}


CLASS_METHODS(RamFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, RamFS, Object)
OVERRIDE_METHOD_IMPL(onMount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onUnmount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyRootNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyNodeForName, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFileInfo, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(setFileInfo, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(createDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(openDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(readDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(checkAccess, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(unlink, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(rename, RamFS, Filesystem)
);
