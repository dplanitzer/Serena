//
//  RamFS.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "RamFS.h"
#include "Lock.h"

#define kMaxFilenameLength  32

typedef struct _GenericArray RamFS_DirectoryHeader;

typedef struct _RamFS_DirectoryEntry {
    InodeRef _Nonnull   node;
    Character           filename[kMaxFilenameLength];
} RamFS_DirectoryEntry;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Directory Inode
////////////////////////////////////////////////////////////////////////////////

OPEN_CLASS_WITH_REF(RamFS_Directory, Inode,
    // [0] "."
    // [1] ".."
    // [2] userEntry0
    // .
    // [n] userEntryN-1
    RamFS_DirectoryHeader       header;
    FilesystemId                mountedFileSys;     // (Directory) The ID of the filesystem mounted on top of this directory; 0 if this directory is not a mount point
);
typedef struct _RamFS_DirectoryMethodTable {
    InodeMethodTable    super;
} RamFS_DirectoryMethodTable;

static ErrorCode DirectoryNode_AddWellKnownEntry(RamFS_DirectoryRef _Nonnull self, const Character* _Nonnull pName, InodeRef _Nonnull pNode);


static ErrorCode DirectoryNode_Create(RamFSRef _Nonnull self, InodeId id, RamFS_DirectoryRef _Nullable pParentDir, FilePermissions permissions, User user, RamFS_DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    RamFS_DirectoryRef pDir;

    try(Inode_AbstractCreate(&kRamFS_DirectoryClass, kInode_Directory, id, Filesystem_GetId(self), permissions, user, 0ll, (InodeRef*)&pDir));
    try(GenericArray_Init(&pDir->header, sizeof(RamFS_DirectoryEntry), 8));
    try(DirectoryNode_AddWellKnownEntry(pDir, ".", (InodeRef) pDir));
    try(DirectoryNode_AddWellKnownEntry(pDir, "..", (pParentDir) ? (InodeRef) pParentDir : (InodeRef) pDir));
    pDir->mountedFileSys = 0;

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

static ErrorCode DirectoryNode_GetNameOfNode(RamFS_DirectoryRef _Nonnull self, InodeRef _Nonnull pNode, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();
    RamFS_DirectoryHeader* pHeader = &self->header;
    const RamFS_DirectoryEntry* pEntry = NULL;

    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
        const RamFS_DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, RamFS_DirectoryEntry, i);

        if (Inode_Equals(pCurEntry->node, pNode)) {
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

    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
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

CLASS_IVARS(RamFS, Filesystem,
    RamFS_DirectoryRef _Nonnull root;
    Lock                        lock;           // Shared between filesystem proper and inodes
    Int                         nextAvailableInodeId;
    Bool                        isReadOnly;     // true if mounted read-only; false if mounted read-write
);

static InodeId RamFS_GetNextAvailableInodeId_Locked(RamFSRef _Nonnull self);



// Creates an instance of RAM-FS. RAM-FS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
ErrorCode RamFS_Create(User rootDirUser, RamFSRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    RamFSRef self;

    try(Filesystem_Create(&kRamFSClass, (FilesystemRef*)&self));
    Lock_Init(&self->lock);
    self->nextAvailableInodeId = 0;

    FilePermissions scopePerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    FilePermissions dirPerms = FilePermissions_Make(scopePerms, scopePerms, scopePerms);

    try(DirectoryNode_Create(self, RamFS_GetNextAvailableInodeId_Locked(self), NULL, dirPerms, rootDirUser, &self->root));

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
    Lock_Deinit(&self->lock);
}

static InodeId RamFS_GetNextAvailableInodeId_Locked(RamFSRef _Nonnull self)
{
    return (InodeId) self->nextAvailableInodeId++;
}

// Invoked when an instance of this file system is mounted.
ErrorCode RamFS_onMount(RamFSRef _Nonnull self, const Byte* _Nonnull pParams, ByteCount paramsSize)
{
    return EOK;
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

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
InodeRef _Nonnull RamFS_copyRootNode(RamFSRef _Nonnull self)
{
    // Our root node is a constant field, no need for locking
    return Object_RetainAs(self->root, Inode);
}

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
ErrorCode RamFS_copyParentOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = RamFS_CheckAccess_Locked(self, pNode, user, kFilePermission_Execute);
    if (err == EOK) {
        err = DirectoryNode_CopyParent((RamFS_DirectoryRef) pNode, pOutNode);
    }
    Lock_Unlock(&self->lock);

    return err;
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

    Lock_Lock(&self->lock);
    err = RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Execute);
    if (err == EOK) {
        err = DirectoryNode_CopyNodeForName((RamFS_DirectoryRef) pParentNode, pComponent, pOutNode);
    }
    Lock_Unlock(&self->lock);
    
    return err;
}

ErrorCode RamFS_getNameOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, User user, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Read | kFilePermission_Execute);
    if (err == EOK) {
        err = DirectoryNode_GetNameOfNode((RamFS_DirectoryRef) pParentNode, pNode, pComponent);
    }
    Lock_Unlock(&self->lock);

    return err;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
ErrorCode RamFS_getFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, FileInfo* _Nonnull pOutInfo)
{
    Lock_Lock(&self->lock);
    Inode_GetFileInfo(pNode, pOutInfo);
    Lock_Unlock(&self->lock);

    return EOK;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
ErrorCode RamFS_setFileInfo(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, MutableFileInfo* _Nonnull pInfo)
{
    Lock_Lock(&self->lock);
    Inode_SetFileInfo(pNode, pInfo);
    Lock_Unlock(&self->lock);

    return EOK;
}

// If the node is a directory and another file system is mounted at this directory,
// then this function returns the filesystem ID of the mounted directory; otherwise
// 0 is returned.
FilesystemId RamFS_getFilesystemMountedOnNode(RamFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    Lock_Lock(&self->lock);
    const FilesystemId fsid = (Inode_IsDirectory(pNode)) ? ((RamFS_DirectoryRef)pNode)->mountedFileSys : 0;
    Lock_Unlock(&self->lock);

    return fsid;
}

// Marks the given directory node as a mount point at which the filesystem
// with the given filesystem ID is mounted. Converts the node back into a
// regular directory node if the give filesystem ID is 0.
void RamFS_setFilesystemMountedOnNode(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, FilesystemId fsid)
{
    Lock_Lock(&self->lock);
    ((RamFS_DirectoryRef)pNode)->mountedFileSys = fsid;
    Lock_Unlock(&self->lock);
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions
ErrorCode RamFS_createDirectory(RamFSRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pName, User user, FilePermissions permissions)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    RamFS_DirectoryRef pNewDir;

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
    Lock_Unlock(&self->lock);
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
    Lock_Lock(&self->lock);
    const Int nMaxEntriesToRead = nBytesToRead / sizeof(DirectoryEntry);
    const Int nEntriesRead = Directory_ReadEntries((RamFS_DirectoryRef)Directory_GetInode(pDir), (DirectoryEntry*)pBuffer, (Int) Directory_GetOffset(pDir), nMaxEntriesToRead);
    
    if (nEntriesRead > 0) {
        Directory_IncrementOffset(pDir, nEntriesRead);
    }
    Lock_Unlock(&self->lock);

    return nEntriesRead * sizeof(DirectoryEntry);
}

// Verifies that the given node is accessible assuming the given access mode.
ErrorCode RamFS_checkAccess(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, User user, Int mode)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if ((mode & kAccess_Readable) == kAccess_Readable) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Read);
    }
    if (err == EOK && ((mode & kAccess_Writable) == kAccess_Writable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Write);
    }
    if (err == EOK && ((mode & kAccess_Executable) == kAccess_Executable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Execute);
    }
    Lock_Unlock(&self->lock);

    return err;
}


CLASS_METHODS(RamFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, RamFS, Object)
OVERRIDE_METHOD_IMPL(onMount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyRootNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyParentOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyNodeForName, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFileInfo, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(setFileInfo, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFilesystemMountedOnNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(setFilesystemMountedOnNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(createDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(readDirectory, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(checkAccess, RamFS, Filesystem)

);
