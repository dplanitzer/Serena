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


OPEN_CLASS_WITH_REF(RamFS_Directory, Inode,
    InodeRef _Nullable _Weak    parent; // parent directory; NULL if this is the root node
    DirectoryHeader             header;
);
typedef struct _RamFS_DirectoryMethodTable {
    InodeMethodTable    super;
} RamFS_DirectoryMethodTable;


CLASS_IVARS(RamFS, Filesystem,
    RamFS_DirectoryRef _Nonnull   root;
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Directory Inode
////////////////////////////////////////////////////////////////////////////////


static ErrorCode DirectoryNode_Create(RamFSRef _Nonnull self, RamFS_DirectoryRef _Nullable pParentDir, RamFS_DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    RamFS_DirectoryRef pDir;

    try(Inode_AbstractCreate(&kRamFS_DirectoryClass, kInode_Directory, Filesystem_GetId(self), (InodeRef*)&pDir));
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
    if (pComponent->count > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    DirectoryHeader* pHeader = &self->header;
    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
        const DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, DirectoryEntry, i);

        if (String_EqualsUpToLength(pCurEntry->filename, pComponent->name, __min(pComponent->count, kMaxFilenameLength))) {
            *pOutNode = Object_RetainAs(pCurEntry->node, Inode);
            return EOK;
        }
    }

    *pOutNode = NULL;
    return ENOENT;
}

static ErrorCode DirectoryNode_GetNameOfNode(RamFS_DirectoryRef _Nonnull self, InodeRef _Nonnull pNode, MutablePathComponent* _Nonnull pComponent)
{
    DirectoryHeader* pHeader = &self->header;

    pComponent->name[0] = '\0';
    pComponent->count = 0;

    for (Int i = 0; i < GenericArray_GetCount(pHeader); i++) {
        const DirectoryEntry* pCurEntry = GenericArray_GetRefAt(pHeader, DirectoryEntry, i);

        if (pCurEntry->node == pNode) {
            const ByteCount len = String_LengthUpToLength(pCurEntry->filename, kMaxFilenameLength);

            if (len > pComponent->capacity) {
                return ERANGE;
            }

            String_CopyUpToLength(pComponent->name, pCurEntry->filename, len);
            pComponent->count = len;
            return EOK;
        }
    }

    return ENOENT;
}

static ErrorCode DirectoryNode_AddEntry(RamFS_DirectoryRef _Nonnull self, const Character* _Nonnull pFilename, InodeRef _Nonnull pChildNode)
{
    const ByteCount nameLen = String_Length(pFilename);

    if (nameLen > kMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    DirectoryEntry entry;
    Character* p = String_CopyUpToLength(entry.filename, pFilename, kMaxFilenameLength);
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

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
ErrorCode RamFS_Create(RamFSRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    RamFSRef self;

    try(Filesystem_Create(&kRamFSClass, (FilesystemRef*)&self));
    try(DirectoryNode_Create(self, NULL, (RamFS_DirectoryRef*)&self->root));
    // XXX set up permissions
    // XXX set up user & group id

    // XXX for now (testing)
    RamFS_DirectoryRef pSystemDir;
    RamFS_DirectoryRef pUsersDir;
    RamFS_DirectoryRef pUsersAdminDir;
    RamFS_DirectoryRef pUsersTesterDir;

    try(DirectoryNode_Create(self, (RamFS_DirectoryRef) self->root, &pSystemDir));
    try(DirectoryNode_Create(self, (RamFS_DirectoryRef) self->root, &pUsersDir));
    try(DirectoryNode_Create(self, (RamFS_DirectoryRef) pUsersDir, &pUsersAdminDir));
    try(DirectoryNode_Create(self, (RamFS_DirectoryRef) pUsersDir, &pUsersTesterDir));

    try(DirectoryNode_AddEntry((RamFS_DirectoryRef)self->root, "System", (InodeRef)pSystemDir));
    Object_Release(pSystemDir);
    try(DirectoryNode_AddEntry((RamFS_DirectoryRef) self->root, "Users", (InodeRef)pUsersDir));
    Object_Release(pUsersDir);
    try(DirectoryNode_AddEntry((RamFS_DirectoryRef) pUsersDir, "Admin", (InodeRef)pUsersAdminDir));
    Object_Release(pUsersAdminDir);
    try(DirectoryNode_AddEntry((RamFS_DirectoryRef) pUsersDir, "Tester", (InodeRef)pUsersTesterDir));
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

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
InodeRef _Nonnull RamFS_copyRootNode(RamFSRef _Nonnull self)
{
    return Object_RetainAs(self->root, Inode);
}

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
ErrorCode RamFS_copyParentOfNode(RamFSRef _Nonnull self, InodeRef _Nonnull pNode, InodeRef _Nullable * _Nonnull pOutNode)
{
    return DirectoryNode_CopyParent((RamFS_DirectoryRef) pNode, pOutNode);
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function will always only be called with proper node names. Eg never with
// "." nor "..".
ErrorCode RamFS_copyNodeForName(RamFSRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* pComponent, InodeRef _Nullable * _Nonnull pOutNode)
{
    return DirectoryNode_CopyNodeForName((RamFS_DirectoryRef) pParentNode, pComponent, pOutNode);
}

ErrorCode RamFS_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, MutablePathComponent* _Nonnull pComponent)
{
    return DirectoryNode_GetNameOfNode((RamFS_DirectoryRef) pParentNode, pNode, pComponent);
}

CLASS_METHODS(RamFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, RamFS, Object)
OVERRIDE_METHOD_IMPL(copyRootNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyParentOfNode, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(copyNodeForName, RamFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, RamFS, Filesystem)
);
