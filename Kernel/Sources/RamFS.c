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
// MARK: Disk Node
////////////////////////////////////////////////////////////////////////////////

static ErrorCode RamDiskNode_Create(InodeId id, InodeType type, RamDiskNodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    RamDiskNodeRef self;

    try(kalloc_cleared(sizeof(RamDiskNode), (void**)&self));
    try(kalloc_cleared(sizeof(RamDirectoryContent), (void**)&self->content));
    try(GenericArray_Init(self->content, sizeof(RamDirectoryEntry), 8));
    self->inid = id;
    self->linkCount = 1;
    self->type = type;

    *pOutNode = self;
    return EOK;

catch:
    RamDiskNode_Destroy(self);
    *pOutNode = NULL;
    return err;
}

static void RamDiskNode_Destroy(RamDiskNodeRef _Nullable self)
{
    if (self) {
        GenericArray_Deinit(self->content);
        kfree(self->content);
        self->content = NULL;
        kfree(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// Creates an instance of RamFS. RamFS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
ErrorCode RamFS_Create(User rootDirUser, RamFSRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    RamFSRef self;

    try(Filesystem_Create(&kRamFSClass, (FilesystemRef*)&self));
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->notifier);
    PointerArray_Init(&self->dnodes, 16);
    self->rootDirUser = rootDirUser;
    self->nextAvailableInodeId = 1;
    self->isMounted = false;
    self->isReadOnly = false;

    try(RamFS_FormatWithEmptyFilesystem(self));

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void RamFS_deinit(RamFSRef _Nonnull self)
{
    for(Int i = 0; i < PointerArray_GetCount(&self->dnodes); i++) {
        RamDiskNode_Destroy(PointerArray_GetAtAs(&self->dnodes, i, RamDiskNodeRef));
    }
    PointerArray_Deinit(&self->dnodes);
    ConditionVariable_Deinit(&self->notifier);
    Lock_Deinit(&self->lock);
}

static ErrorCode RamFS_FormatWithEmptyFilesystem(RamFSRef _Nonnull self)
{
    decl_try_err();
    const FilePermissions scopePerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    const FilePermissions dirPerms = FilePermissions_Make(scopePerms, scopePerms, scopePerms);

    try(RamFS_CreateDirectoryDiskNode(self, 0, self->rootDirUser.uid, self->rootDirUser.gid, dirPerms, &self->rootDirId));
    return EOK;

catch:
    return err;
}

static Int RamFS_GetIndexOfDiskNodeForId(RamFSRef _Nonnull self, InodeId id)
{
    for (Int i = 0; i < PointerArray_GetCount(&self->dnodes); i++) {
        RamDiskNodeRef pCurDiskNode = PointerArray_GetAtAs(&self->dnodes, i, RamDiskNodeRef);

        if (pCurDiskNode->inid == id) {
            return i;
        }
    }
    return -1;
}

// Invoked when Filesystem_AllocateNode() is called. Subclassers should
// override this method to allocate the on-disk representation of an inode
// of the given type.
ErrorCode RamFS_onAllocateNodeOnDisk(RamFSRef _Nonnull self, InodeType type, void* _Nullable pContext, InodeId* _Nonnull pOutId)
{
    decl_try_err();
    const InodeId id = (InodeId) self->nextAvailableInodeId++;
    RamDiskNodeRef pDiskNode = NULL;

    try(RamDiskNode_Create(id, type, &pDiskNode));
    try(PointerArray_Add(&self->dnodes, pDiskNode));
    *pOutId = id;

    return EOK;

catch:
    RamDiskNode_Destroy(pDiskNode);
    *pOutId = 0;
    return err;
}

// Invoked when Filesystem_AcquireNodeWithId() needs to read the requested inode
// off the disk. The override should read the inode data from the disk,
// create and inode instance and fill it in with the data from the disk and
// then return it. It should return a suitable error and NULL if the inode
// data can not be read off the disk.
ErrorCode RamFS_onReadNodeFromDisk(RamFSRef _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const Int dIdx = RamFS_GetIndexOfDiskNodeForId(self, id);
    if (dIdx < 0) throw(ENOENT);
    const RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);

    return Inode_Create(
        Filesystem_GetId(self),
        id,
        pDiskNode->type,
        pDiskNode->linkCount,
        pDiskNode->uid,
        pDiskNode->gid,
        pDiskNode->permissions,
        pDiskNode->size,
        pDiskNode->content,
        pOutNode);

catch:
    return err;
}

// Invoked when the inode is relinquished and it is marked as modified. The
// filesystem override should write the inode meta-data back to the 
// corresponding disk node.
ErrorCode RamFS_onWriteNodeToDisk(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    decl_try_err();
    const Int dIdx = RamFS_GetIndexOfDiskNodeForId(self, Inode_GetId(pNode));
    if (dIdx < 0) throw(ENOENT);
    RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);

    pDiskNode->linkCount = Inode_GetLinkCount(pNode);
    pDiskNode->uid = Inode_GetUserId(pNode);
    pDiskNode->gid = Inode_GetGroupId(pNode);
    pDiskNode->permissions = Inode_GetFilePermissions(pNode);
    pDiskNode->size = Inode_GetFileSize(pNode);
    return EOK;

catch:
    return err;
}

// Invoked when Filesystem_RelinquishNode() has determined that the inode is
// no longer being referenced by any directory and that the on-disk
// representation should be deleted from the disk and deallocated. This
// operation is assumed to never fail.
void RamFS_onRemoveNodeFromDisk(RamFSRef _Nonnull self, InodeId id)
{
    const Int dIdx = RamFS_GetIndexOfDiskNodeForId(self, id);

    if (dIdx >= 0) {
        RamDiskNodeRef pDiskNode = PointerArray_GetAtAs(&self->dnodes, dIdx, RamDiskNodeRef);

        RamDiskNode_Destroy(pDiskNode);
        PointerArray_RemoveAt(&self->dnodes, dIdx);
    }
}

// Checks whether the given user should be granted access to the given node based
// on the requested permission. Returns EOK if access should be granted and a suitable
// error code if it should be denied.
static ErrorCode RamFS_CheckAccess_Locked(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FilePermissions permission)
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

    if (self->isMounted) {
        throw(EIO);
    }

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
/*
    Lock_Lock(&self->lock);
    if (!self->isMounted) {
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
    */
    return err;
}


// Returns the root node of the filesystem if the filesystem is currently in
// mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
ErrorCode RamFS_acquireRootNode(RamFSRef _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    return Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirId, NULL, pOutNode);
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function has to support the special names "." (node itself) and ".."
// (parent of node) in addition to "regular" filenames. If 'pParentNode' is
// the root node of the filesystem and 'pComponent' is ".." then 'pParentNode'
// should be returned. If the path component name is longer than what is
// supported by the file system, ENAMETOOLONG should be returned.
ErrorCode RamFS_acquireNodeForName(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    decl_try_err();

    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Execute));

    if (pComponent->count > kMaxFilenameLength) {
        throw(ENAMETOOLONG);
    }

    RamDirectoryContentRef pDirContent = Inode_GetRefConAs(pParentNode, RamDirectoryContentRef);
    const RamDirectoryEntry* pEntry = NULL;

    for (Int i = 0; i < GenericArray_GetCount(pDirContent); i++) {
        const RamDirectoryEntry* pCurEntry = GenericArray_GetRefAt(pDirContent, RamDirectoryEntry, i);

        if (String_EqualsUpTo(pCurEntry->filename, pComponent->name, __min(pComponent->count, kMaxFilenameLength))) {
            pEntry = pCurEntry;
            break;
        }
    }
    if (pEntry == NULL) {
        throw(ENOENT);
    }

    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, pEntry->id, NULL, pOutNode));
    return EOK;

catch:
    *pOutNode = NULL;
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
ErrorCode RamFS_getNameOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();

    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Read | kFilePermission_Execute));

    RamDirectoryContentRef pDirContent = Inode_GetRefConAs(pParentNode, RamDirectoryContentRef);
    const RamDirectoryEntry* pEntry = NULL;

    for (Int i = 0; i < GenericArray_GetCount(pDirContent); i++) {
        const RamDirectoryEntry* pCurEntry = GenericArray_GetRefAt(pDirContent, RamDirectoryEntry, i);

        if (pCurEntry->id == id) {
            pEntry = pCurEntry;
            break;
        }
    }
    if (pEntry == NULL) {
        throw(ENOENT);
    }

    const ByteCount len = String_LengthUpTo(pEntry->filename, kMaxFilenameLength);
    if (len > pComponent->capacity) {
        throw(ERANGE);
    }

    String_CopyUpTo(pComponent->name, pEntry->filename, len);
    pComponent->count = len;
    return EOK;

catch:
    pComponent->count = 0;
    return err;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
ErrorCode RamFS_getFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo)
{
    Inode_GetFileInfo(pNode, pOutInfo);
    return EOK;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
ErrorCode RamFS_setFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();

    if (self->isReadOnly) {
        throw(EROFS);
    }
    try(Inode_SetFileInfo(pNode, user, pInfo));

catch:
    return err;
}

static ErrorCode RamFS_AddDirectoryEntry(RamFSRef _Nonnull self, InodeRef _Nonnull pDirNode, const PathComponent* _Nonnull pName, InodeId id)
{
    if (pName->count > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    RamDirectoryEntry entry;
    Character* p = String_CopyUpTo(entry.filename, pName->name, pName->count);
    while (p < &entry.filename[kMaxFilenameLength]) *p++ = '\0';
    entry.id = id;

    ErrorCode err = EOK;
    RamDirectoryContentRef pDirContent = Inode_GetRefConAs(pDirNode, RamDirectoryContentRef);
    GenericArray_InsertAt(err, pDirContent, entry, RamDirectoryEntry, GenericArray_GetCount(pDirContent));
    if (err == EOK) {
        Inode_IncrementFileSize(pDirNode, sizeof(entry));
        Inode_MarkModified(pDirNode);
    }

    return err;
}

static ErrorCode RamFS_CreateDirectoryDiskNode(RamFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId)
{
    decl_try_err();
    InodeRef pDirNode = NULL;

    try(Filesystem_AllocateNode((FilesystemRef)self, kInode_Directory, uid, gid, permissions, NULL, &pDirNode));
    const InodeId id = Inode_GetId(pDirNode);

    try(RamFS_AddDirectoryEntry(self, pDirNode, &kPathComponent_Self, id));
    try(RamFS_AddDirectoryEntry(self, pDirNode, &kPathComponent_Parent, (parentId > 0) ? parentId : id));
    
    Filesystem_RelinquishNode((FilesystemRef)self, pDirNode);
    *pOutId = id;
    return EOK;

catch:
    Filesystem_RelinquishNode((FilesystemRef)self, pDirNode);
    *pOutId = 0;
    return err;
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions. Returns EEXIST if a node with
// the given name already exists.
ErrorCode RamFS_createDirectory(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions)
{
    decl_try_err();

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // We must have write permissions for 'pParentNode'
    try(RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // Make sure that 'pParentNode' has no entry named 'pName'
    RamDirectoryContentRef pParentDirContent = Inode_GetRefConAs(pParentNode, RamDirectoryContentRef);
    for (Int i = 0; i < GenericArray_GetCount(pParentDirContent); i++) {
        const RamDirectoryEntry* pCurEntry = GenericArray_GetRefAt(pParentDirContent, RamDirectoryEntry, i);

        if (String_EqualsUpTo(pCurEntry->filename, pName->name, __min(pName->count, kMaxFilenameLength))) {
            throw(EEXIST);
        }
    }

    InodeId newDirId = 0;
    try(RamFS_CreateDirectoryDiskNode(self, Inode_GetId(pParentNode), user.uid, user.gid, permissions, &newDirId));
    try(RamFS_AddDirectoryEntry(self, pParentNode, pName, newDirId));
    return EOK;

catch:
    // XXX Unlink new dir disk node
    return err;
}

// Opens the directory represented by the given node. Returns a directory
// descriptor object which is the I/O channel that allows you to read the
// directory content.
ErrorCode RamFS_openDirectory(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    try(Inode_CheckAccess(pDirNode, user, kFilePermission_Read));
    try(Directory_Create((FilesystemRef)self, pDirNode, pOutDir));

catch:
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
    RamDirectoryContentRef pDirContent = Inode_GetRefConAs(Directory_GetInode(pDir), RamDirectoryContentRef);
    DirectoryEntry* pOut = (DirectoryEntry*)pBuffer;
    const Int startIndex = (Int) Directory_GetOffset(pDir);
    const Int nMaxEntriesToRead = nBytesToRead / sizeof(DirectoryEntry);
    const Int readLimit = __min(GenericArray_GetCount(pDirContent), startIndex + nMaxEntriesToRead);
    Int nEntriesRead = 0;

    for (Int i = startIndex; i < readLimit; i++) {
        const RamDirectoryEntry* pCurEntry = GenericArray_GetRefAt(pDirContent, RamDirectoryEntry, i);

        pOut->inodeId = pCurEntry->id;
        String_CopyUpTo(pOut->name, pCurEntry->filename, kMaxFilenameLength);

        pOut++;
        nEntriesRead++;
    }

    if (nEntriesRead > 0) {
        Directory_IncrementOffset(pDir, nEntriesRead);
    }

    return nEntriesRead * sizeof(DirectoryEntry);
}

// Verifies that the given node is accessible assuming the given access mode.
ErrorCode RamFS_checkAccess(RamFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, Int mode)
{
    decl_try_err();

    if ((mode & kAccess_Readable) == kAccess_Readable) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Read);
    }
    if (err == EOK && ((mode & kAccess_Writable) == kAccess_Writable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Write);
    }
    if (err == EOK && ((mode & kAccess_Executable) == kAccess_Executable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Execute);
    }

    return err;
}

// Unlink the node identified by the path component 'pName' and which is an
// immediate child of the (directory) node 'pParentNode'. The parent node is
// guaranteed to be a node owned by the filesystem.
// This function must validate that a directory entry with the given name
// actually exists, is a file or an empty directory before it attempts to
// remove the entry from the parent node.
ErrorCode RamFS_unlink(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user)
{
    // XXX implement me
    return EACCESS;
}

// Renames the node with name 'pName' and which is an immediate child of the
// node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
// the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
ErrorCode RamFS_rename(RamFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pNewParentNode, User user)
{
    // XXX implement me
    return EACCESS;
}


CLASS_METHODS(RamFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, RamFS, Object)
OVERRIDE_METHOD_IMPL(onAllocateNodeOnDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onReadNodeFromDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onWriteNodeToDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onRemoveNodeFromDisk, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onMount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(onUnmount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireRootNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireNodeForName, RamFS, Filesystem)
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
