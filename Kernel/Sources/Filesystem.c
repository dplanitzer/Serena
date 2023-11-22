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

ErrorCode Inode_AbstractCreate(ClassRef pClass, Int8 type, FilePermissions permissions, User user, FilesystemId fsid, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef pNode;

    try(_Object_Create(pClass, 0, (ObjectRef*)&pNode));
    pNode->type = type;
    pNode->permissions = permissions;
    pNode->uid = user.uid;
    pNode->gid = user.gid;
    pNode->fsid = fsid;
    pNode->u.mountedFileSys = 0;
    *pOutNode = pNode;
    return EOK;

catch:
    *pOutNode = NULL;
    return err;
}

// Returns a strong reference to the filesystem that owns the given note. Returns
// NULL if the filesystem isn't mounted.
FilesystemRef Inode_CopyFilesystem(InodeRef _Nonnull self)
{
    return FilesystemManager_CopyFilesystemForId(gFilesystemManager, Inode_GetFilesystemId(self));
}

// If the node is a directory and another file system is mounted at this directory,
// then this function returns the filesystem ID of the mounted directory; otherwise
// 0 is returned.
FilesystemId Inode_GetMountedFilesystemId(InodeRef _Nonnull self)
{
    if (self->type == kInode_Directory && self->u.mountedFileSys > 0) {
        return self->u.mountedFileSys;
    } else {
        return 0;
    }
}

// Marks the given node as a mount point at which the filesystem with the given
// filesystem ID is mounted. Converts the node back into a regular directory
// node if the give filesystem ID is 0.
void Inode_SetMountedFilesystemId(InodeRef _Nonnull self, FilesystemId fsid)
{
    assert(self->type == kInode_Directory);
    self->u.mountedFileSys = fsid;
}

// Returns true if the receiver is a child node of 'pOtherNode' or it is 'pOtherNode';
// otherwise returns false. An Error is returned if the relationship can not be
// successful established because eg the function detects that the node or one of
// its parents is owned by a file system that is not currently mounted or because
// of a lack of permissions.
ErrorCode Inode_IsChildOfNode(InodeRef _Nonnull self, InodeRef _Nonnull pOtherNode, Bool* _Nonnull pOutResult)
{
    if (self == pOtherNode) {
        *pOutResult = true;
        return EOK;
    }

    *pOutResult = false;
    InodeRef pCurNode = Object_RetainAs(self, Inode);
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

// Returns EOK if the given user has the permission to access/user the node the
// way implied by 'permission'; a suitable error code otherwise.
ErrorCode Inode_CheckAccess(InodeRef _Nonnull self, User user, FilePermissions permission)
{
    // XXX revisit this once we put a proper user permission model in place
    if (user.uid == kRootUserId) {
        return EOK;
    }

    FilePermissions reqPerms = 0;

    if (Inode_GetUserId(self) == user.uid) {
        reqPerms = FilePermissions_Make(0, 0, permission);
    }
    else if (Inode_GetGroupId(self) == user.gid) {
        reqPerms = FilePermissions_Make(0, permission, 0);
    }
    else {
        reqPerms = FilePermissions_Make(permission, 0, 0);
    }

    if ((Inode_GetPermissions(self) & reqPerms) != 0) {
        return EOK;
    }

    return EACCESS;
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
    pComponent->count = 0;
    return ENOENT;
}

CLASS_METHODS(Filesystem, IOResource,
METHOD_IMPL(copyRootNode, Filesystem)
METHOD_IMPL(copyParentOfNode, Filesystem)
METHOD_IMPL(copyNodeForName, Filesystem)
METHOD_IMPL(getNameOfNode, Filesystem)
);
