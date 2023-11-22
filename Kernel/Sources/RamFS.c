//
//  RamFS.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "RamFS.h"

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
    InodeRef _Nullable _Weak    parent; // parent directory; NULL if this is the root node
    DirectoryHeader             header;
);
typedef struct _RamFS_DirectoryMethodTable {
    InodeMethodTable    super;
} RamFS_DirectoryMethodTable;


static ErrorCode DirectoryNode_Create(RamFSRef _Nonnull self, RamFS_DirectoryRef _Nullable pParentDir, FilePermissions permissions, User user, RamFS_DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    RamFS_DirectoryRef pDir;

    try(Inode_AbstractCreate(&kRamFS_DirectoryClass, kInode_Directory, permissions, user, Filesystem_GetId(self), (InodeRef*)&pDir));
    try(GenericArray_Init(&pDir->header, sizeof(DirectoryEntry), 4));
    pDir->parent = (InodeRef) pParentDir;
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

        if (pCurEntry->node == pNode) {
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

static ErrorCode DirectoryNode_AddEntry(RamFS_DirectoryRef _Nonnull self, const Character* _Nonnull pFilename, InodeRef _Nonnull pChildNode)
{
    const ByteCount nameLen = String_Length(pFilename);

    if (nameLen > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    DirectoryEntry entry;
    Character* p = String_CopyUpTo(entry.filename, pFilename, kMaxFilenameLength);
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
    Bool                        isReadOnly;     // true if mounted read-only; false if mounted read-write
);


// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
ErrorCode RamFS_Create(RamFSRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    RamFSRef self;
    User user = {kRootUserId, kRootGroupId};
    FilePermissions scopePerms = kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute;
    FilePermissions dirPerms = FilePermissions_Make(scopePerms, scopePerms, scopePerms);

    try(Filesystem_Create(&kRamFSClass, (FilesystemRef*)&self));
    try(DirectoryNode_Create(self, NULL, dirPerms, user, &self->root));
    // XXX set up permissions
    // XXX set up user & group id

    // XXX for now (testing)
    RamFS_DirectoryRef pSystemDir;
    RamFS_DirectoryRef pUsersDir;
    RamFS_DirectoryRef pUsersAdminDir;
    RamFS_DirectoryRef pUsersTesterDir;

    try(DirectoryNode_Create(self, self->root, dirPerms, user, &pSystemDir));
    try(DirectoryNode_Create(self, self->root, dirPerms, user, &pUsersDir));
    try(DirectoryNode_Create(self, pUsersDir, dirPerms, user, &pUsersAdminDir));
    try(DirectoryNode_Create(self, pUsersDir, dirPerms, user, &pUsersTesterDir));

    try(DirectoryNode_AddEntry(self->root, "System", (InodeRef)pSystemDir));
    Object_Release(pSystemDir);
    try(DirectoryNode_AddEntry(self->root, "Users", (InodeRef)pUsersDir));
    Object_Release(pUsersDir);
    try(DirectoryNode_AddEntry(pUsersDir, "Admin", (InodeRef)pUsersAdminDir));
    Object_Release(pUsersAdminDir);
    try(DirectoryNode_AddEntry(pUsersDir, "Tester", (InodeRef)pUsersTesterDir));
    Object_Release(pUsersTesterDir);
    // XXX for now (testing)

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
    return Object_RetainAs(self->root, Inode);
}

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
ErrorCode RamFS_copyParentOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    ErrorCode err = RamFS_CheckAccess_Locked(self, pNode, user, kFilePermission_Execute);
    if (err != EOK) {
        return err;
    }

    return DirectoryNode_CopyParent((RamFS_DirectoryRef) pNode, pOutNode);
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function will always only be called with proper node names. Eg never with
// "." nor "..".
ErrorCode RamFS_copyNodeForName(RamFSRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* pComponent, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    ErrorCode err = RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Execute);
    if (err != EOK) {
        return err;
    }

    return DirectoryNode_CopyNodeForName((RamFS_DirectoryRef) pParentNode, pComponent, pOutNode);
}

ErrorCode RamFS_getNameOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, User user, MutablePathComponent* _Nonnull pComponent)
{
    ErrorCode err = RamFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Read | kFilePermission_Execute);
    if (err != EOK) {
        return err;
    }

    return DirectoryNode_GetNameOfNode((RamFS_DirectoryRef) pParentNode, pNode, pComponent);
}

CLASS_METHODS(RamFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, RamFS, Object)
OVERRIDE_METHOD_IMPL(copyRootNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyParentOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyNodeForName, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, RamFS, Filesystem)
);
