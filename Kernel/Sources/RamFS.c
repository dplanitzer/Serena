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

typedef struct _GenericArray DirectoryHeader;

typedef struct _DirectoryEntry {
    InodeRef _Nonnull   node;
    Character           filename[kMaxFilenameLength];
} DirectoryEntry;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Directory Inode
////////////////////////////////////////////////////////////////////////////////

OPEN_CLASS_WITH_REF(RamFS_Directory, Inode,
    InodeRef _Nullable _Weak    parent;             // Parent directory; NULL if this is the root node
    DirectoryHeader             header;
    FilesystemId                mountedFileSys;     // (Directory) The ID of the filesystem mounted on top of this directory; 0 if this directory is not a mount point
);
typedef struct _RamFS_DirectoryMethodTable {
    InodeMethodTable    super;
} RamFS_DirectoryMethodTable;


static ErrorCode DirectoryNode_Create(RamFSRef _Nonnull self, InodeId id, RamFS_DirectoryRef _Nullable pParentDir, FilePermissions permissions, User user, RamFS_DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    RamFS_DirectoryRef pDir;

    try(Inode_AbstractCreate(&kRamFS_DirectoryClass, kInode_Directory, id, Filesystem_GetId(self), permissions, user, (InodeRef*)&pDir));
    try(GenericArray_Init(&pDir->header, sizeof(DirectoryEntry), 4));
    pDir->parent = (InodeRef) pParentDir;
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
    if (self->parent) {
        *pOutParent = Object_RetainAs(self->parent, Inode);
        return EOK;
    } else {
        *pOutParent = NULL;
        return ENOENT;
    }
}

static ErrorCode DirectoryNode_CopyNodeForName(RamFS_DirectoryRef _Nonnull self, const PathComponent* pComponent, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    DirectoryHeader* pHeader = &self->header;
    const DirectoryEntry* pEntry = NULL;

    if (pComponent->count > kMaxFilenameLength) {
        throw(ENAMETOOLONG);
    }

    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
        const DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, DirectoryEntry, i);

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
    DirectoryHeader* pHeader = &self->header;
    const DirectoryEntry* pEntry = NULL;

    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
        const DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, DirectoryEntry, i);

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

static ErrorCode DirectoryNode_AddEntry(RamFS_DirectoryRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull pChildNode)
{
    if (pName->count > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    DirectoryEntry entry;
    Character* p = String_CopyUpTo(entry.filename, pName->name, pName->count);
    while (p < &entry.filename[kMaxFilenameLength]) *p++ = '\0';
    entry.node = Object_RetainAs(pChildNode, Inode);

    ErrorCode err = EOK;
    GenericArray_InsertAt(err, &self->header, entry, DirectoryEntry, GenericArray_GetCount(&self->header));

    return err;
}


void RamFS_Directory_deinit(RamFS_DirectoryRef _Nonnull self)
{
    self->parent = NULL;
    GenericArray_Deinit(&self->header);
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
// function will always only be called with proper node names. Eg never with
// "." nor "..".
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


CLASS_METHODS(RamFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, RamFS, Object)
OVERRIDE_METHOD_IMPL(onMount, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyRootNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyParentOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyNodeForName, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFileInfo, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFilesystemMountedOnNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(setFilesystemMountedOnNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(createDirectory, RamFS, Filesystem)
);
