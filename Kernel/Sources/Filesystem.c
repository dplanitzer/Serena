//
//  Filesystem.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Filesystem.h"
#include "FilesystemManager.h"

////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Inode
////////////////////////////////////////////////////////////////////////////////

ErrorCode Inode_Create(Int8 type, FilesystemId fsid, void* _Nullable refcon, InodeRef _Nullable * _Nonnull pOutInode)
{
    decl_try_err();
    InodeRef pNode;

    try(Object_CreateWithExtraBytes(Inode, 0, &pNode));
    pNode->type = type;
    pNode->permissions = 0;
    pNode->uid = 0;
    pNode->gid = 0;
    pNode->fsid = fsid;
    pNode->u.mountedFileSys = 0;
    pNode->refcon = refcon;
    *pOutInode = pNode;
    return EOK;

catch:
    *pOutInode = NULL;
    return err;
}

ErrorCode Inode_AbstractCreate(ClassRef pClass, Int8 type, FilesystemId fsid, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef pNode;

    try(_Object_Create(pClass, 0, (ObjectRef*)&pNode));
    pNode->type = type;
    pNode->permissions = 0;
    pNode->uid = 0;
    pNode->gid = 0;
    pNode->fsid = fsid;
    pNode->u.mountedFileSys = 0;
    pNode->refcon = NULL;
    *pOutNode = pNode;
    return EOK;

catch:
    *pOutNode = NULL;
    return err;
}

// Returns a strong reference to the filesystem that owns the given note. Returns
// NULL if the filesystem isn't mounted.
FilesystemRef Inode_CopyFilesystem(InodeRef _Nonnull pNode)
{
    return FilesystemManager_CopyFilesystemForId(gFilesystemManager, Inode_GetFilesystemId(pNode));
}

// If the node is a directory and another file system is mounted at this directory,
// then this function returns the filesystem ID of the mounted directory; otherwise
// 0 is returned.
FilesystemId Inode_GetMountedFilesystemId(InodeRef _Nonnull pNode)
{
    if (pNode->type == kInode_Directory && pNode->u.mountedFileSys > 0) {
        return pNode->u.mountedFileSys;
    } else {
        return 0;
    }
}

// Marks the given node as a mount point at which the filesystem with the given
// filesystem ID is mounted. Converts the node back into a regular directory
// node if the give filesystem ID is 0.
void Inode_SetMountedFilesystemId(InodeRef _Nonnull pNode, FilesystemId fsid)
{
    assert(pNode->type == kInode_Directory);
    pNode->u.mountedFileSys = fsid;
}

ErrorCode Inode_IsChildOfNode(InodeRef _Nonnull pChildNode, InodeRef _Nonnull pOtherNode, Bool* _Nonnull pOutResult)
{
    if (pChildNode == pOtherNode) {
        *pOutResult = true;
        return EOK;
    }

    *pOutResult = false;
    InodeRef pCurNode = Object_RetainAs(pChildNode, Inode);
    FilesystemRef pCurFilesystem = Inode_CopyFilesystem(pCurNode);

    while (true) {
        InodeRef pParentNode;
        ErrorCode err = Filesystem_CopyParentOfNode(pCurFilesystem, pCurNode, &pParentNode);
        Object_Release(pCurNode);

        if (err == ENOENT) {
            InodeRef pMountingDir;
            FilesystemRef pMountingFilesystem;

            err = FilesystemManager_CopyNodeAndFilesystemMountingFilesystemId(gFilesystemManager, Filesystem_GetId(pCurFilesystem), &pMountingDir, &pMountingFilesystem);
            if (err != EOK) {
                Object_Release(pCurFilesystem);
                break;
            }

            try_bang(Filesystem_CopyParentOfNode(pMountingFilesystem, pMountingDir, &pParentNode));
        }

        if (pParentNode == pOtherNode) {
            *pOutResult = true;
            Object_Release(pParentNode);
            Object_Release(pCurFilesystem);
            break;
        }

        pCurNode = pParentNode;
    }

    return EOK;
}

void Inode_deinit(InodeRef _Nonnull self)
{
}

CLASS_METHODS(Inode, Object,
OVERRIDE_METHOD_IMPL(deinit, Inode, Object)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// Returns the next available FSID.
static FilesystemId Filesystem_GetNextAvailableId(void)
{
    static volatile AtomicInt gNextAvailableId = 0;
    return AtomicInt_Increment(&gNextAvailableId);
}

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
ErrorCode Filesystem_Create(ClassRef pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    FilesystemRef pFileSys;

    try(_Object_Create(pClass, 0, (ObjectRef*)&pFileSys));
    pFileSys->fsid = Filesystem_GetNextAvailableId();
    *pOutFileSys = pFileSys;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
InodeRef _Nonnull Filesystem_copyRootNode(FilesystemRef _Nonnull self)
{
    abort();
    // NOT REACHED
    return NULL;
}

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
ErrorCode Filesystem_copyParentOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, InodeRef _Nullable * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function will always only be called with proper node names. Eg never with
// "." nor "..".
ErrorCode Filesystem_copyNodeForName(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* pComponent, InodeRef _Nullable * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

ErrorCode Filesystem_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, MutablePathComponent* _Nonnull pComponent)
{
    pComponent->name[0] = '\0';
    pComponent->count = 0;
    return ENOENT;
}

CLASS_METHODS(Filesystem, IOResource,
METHOD_IMPL(copyRootNode, Filesystem)
METHOD_IMPL(copyParentOfNode, Filesystem)
METHOD_IMPL(copyNodeForName, Filesystem)
METHOD_IMPL(getNameOfNode, Filesystem)
);
