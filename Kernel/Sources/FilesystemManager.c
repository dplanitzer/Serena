//
//  FilesystemManager.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/09/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include "Lock.h"
#include "ProcessManager.h"


typedef struct _Mountpoint {
    ListNode                node;
    
    // The FS that we are mounting...
    FilesystemRef _Nonnull  mountedFilesystem;

    // ...in this place
    InodeRef _Nullable      mountingNode;       // Only ever NULL for the root FS
} Mountpoint;

typedef struct _FilesystemManager {
    Lock                    lock;
    List                    fileSystems;
    Mountpoint* _Nonnull    root;
} FilesystemManager;


FilesystemManagerRef    gFilesystemManager;


// Creates the filesystem manager. The provided filesystem becomes the root of
// the global filesystem. Also known as "/".
ErrorCode FilesystemManager_Create(FilesystemRef _Nonnull pRootFileSys, FilesystemManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    FilesystemManagerRef pManager;
    
    try(kalloc(sizeof(FilesystemManager), (void**) &pManager));
    Lock_Init(&pManager->lock);
    List_Init(&pManager->fileSystems);

    try(kalloc_cleared(sizeof(Mountpoint), (void**) &pManager->root));
    pManager->root->mountedFilesystem = Object_RetainAs(pRootFileSys, Filesystem);
    pManager->root->mountingNode = NULL;
    
    List_InsertAfter(&pManager->fileSystems, &pManager->root->node, NULL);

    Byte dummy;
    try(Filesystem_OnMount(pRootFileSys, &dummy, 0));

    *pOutManager = pManager;
    return EOK;

catch:
    *pOutManager = NULL;
    return err;
}

// Returns the mountpoint data structure for the given filesystem ID. NULL is
// returned if the given filesystem is not mounted.
static Mountpoint* _Nullable FilesystemManager_GetMountpointForFilesystemId_Locked(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid)
{
    List_ForEach(&pManager->fileSystems, Mountpoint, {
        FilesystemRef pCurFileSys = pCurNode->mountedFilesystem;

        if (Filesystem_GetId(pCurFileSys) == fsid) {
            return pCurNode;
        }
    });

    return NULL;
}

// Returns the mountpoint data structure for the given node if it is a mountpoint.
// Returns NULL if it is not.
static Mountpoint* _Nullable FilesystemManager_GetMountpointForInode_Locked(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull pNode)
{
    List_ForEach(&pManager->fileSystems, Mountpoint, {
        if (Inode_Equals(pNode, pCurNode->mountingNode)) {
            return pCurNode;
        }
    });

    return NULL;
}

// Returns a strong reference to the root of the global filesystem.
FilesystemRef _Nonnull FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull pManager)
{
    // rootFileSys is a constant value, so no locking needed
    return Object_RetainAs(pManager->root->mountedFilesystem, Filesystem);
}

// Returns the node that represents the root of the global filesystem.
InodeRef _Nonnull FilesystemManager_CopyRootNode(FilesystemManagerRef _Nonnull pManager)
{
    // rootNode is a constant value, so no locking needed
    return Object_RetainAs(pManager->root->mountingNode, Inode);
}

// Returns the filesystem for the given filesystem ID. NULL is returned if no
// file system for the given ID is registered/mounted anywhere in the global
// namespace.
FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid)
{
    Lock_Lock(&pManager->lock);
    const Mountpoint* pMountpoint = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, fsid);
    const FilesystemRef pFileSys = (pMountpoint) ? Object_RetainAs(pMountpoint->mountedFilesystem, Filesystem) : NULL;
    Lock_Unlock(&pManager->lock);

    return pFileSys;
}

// Returns the mountpoint information of the node and filesystem that mount the
// given filesystem. ENOENT and NULLs are returned if the filesystem was never
// mounted or is no longer mounted. EOK and NULLs are returned if 'pFileSys' is
// the root filesystem (it has no parent file system).
ErrorCode FilesystemManager_CopyMountpointOfFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nullable * _Nonnull pOutMountingNode, FilesystemRef _Nullable * _Nonnull pOutMountingFilesystem)
{
    Lock_Lock(&pManager->lock);
    const Mountpoint* pMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, Filesystem_GetId(pFileSys));
    ErrorCode err;

    if (pMount) {
        *pOutMountingNode = (pMount->mountingNode) ? Object_RetainAs(pMount->mountingNode, Inode) : NULL;
        *pOutMountingFilesystem = (pMount->mountingNode) ? Object_RetainAs(FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, Inode_GetFilesystemId(pMount->mountingNode))->mountedFilesystem, Filesystem) : NULL;
        err = EOK;
    } else {
        *pOutMountingNode = NULL;
        *pOutMountingFilesystem = NULL;
        err = ENOENT;
    }
    Lock_Unlock(&pManager->lock);

    return err;
}

// Checks whether the given node is a mount point and returns the mount point
// information if it is. Otherwise returns false. The returned mount point
// information is the filesystem and its root node, that is mounted at the given
// node.
Bool FilesystemManager_IsNodeMountpoint(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull pNode, FilesystemRef _Nullable * _Nonnull pMountedFilesystem, InodeRef _Nullable * _Nonnull pMountedRootNode)
{
    Lock_Lock(&pManager->lock);
    const Bool isMountpoint = Inode_IsMountpoint(pNode);
    if (isMountpoint) {
        const Mountpoint* pMountpoint = FilesystemManager_GetMountpointForInode_Locked(pManager, pNode);

        *pMountedFilesystem = Object_RetainAs(pMountpoint->mountedFilesystem, Filesystem);
        *pMountedRootNode = Filesystem_CopyRootNode(*pMountedFilesystem);
    } else {
        *pMountedFilesystem = NULL;
        *pMountedRootNode = NULL;
    }
    Lock_Unlock(&pManager->lock);
    
    return isMountpoint;
}

// Mounts the given filesystem at the given node. The node must be a directory
// node. A filesystem instance may be mounted at at most one directory.
ErrorCode FilesystemManager_Mount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, const Byte* _Nonnull pParams, ByteCount paramsSize, InodeRef _Nonnull pDirNode)
{
    decl_try_err();
    const FilesystemId mountingFsid = Filesystem_GetId(pFileSys);
    const FilesystemId mountedOnFsid = Inode_GetFilesystemId(pDirNode);

    // Make sure that 'pDirNode' isn't owned by the filesystem we want to mount
    // and that the filesystem instance we want to mount isn't already mounted
    // somewhere else 
    Lock_Lock(&pManager->lock);
    if (mountedOnFsid == mountingFsid) {
        throw(EINVAL);
    }
    
    if (FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, mountingFsid)) {
        throw(EINVAL);
    }


    // Make sure that filesystem that owns 'pDirNode' is still mounted and get it.
    const Mountpoint* pDirNodeMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, mountedOnFsid);
    if (pDirNodeMount == NULL) {
        throw(EINVAL);
    }


    // Notify the filesystem that we are mounting it
    try(Filesystem_OnMount(pFileSys, pParams, paramsSize));


    // Update our mount table
    Mountpoint* pMount;
    throw(kalloc_cleared(sizeof(Mountpoint), (void**) &pMount));
    pMount->mountedFilesystem = Object_RetainAs(pFileSys, Filesystem);
    pMount->mountingNode = Object_RetainAs(pDirNode, Inode);

    List_InsertAfterLast(&pManager->fileSystems, &pMount->node);
    Inode_SetMountpoint(pDirNode, true);

    Lock_Unlock(&pManager->lock);

    return EOK;

catch:
    Lock_Unlock(&pManager->lock);
    return err;
}

// Unmounts the given filesystem from the given directory.
ErrorCode FilesystemManager_Unmount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull pDirNode)
{
    decl_try_err();

    Lock_Lock(&pManager->lock);
    const FilesystemId unmountingFsid = Filesystem_GetId(pFileSys);


    // Make sure that 'pFileSys' is actually mounted at 'pDirNode'
    Mountpoint* pMount = NULL;
    
    if (Inode_IsMountpoint(pDirNode)) {
        pMount = FilesystemManager_GetMountpointForInode_Locked(pManager, pDirNode);
    }
    if (pMount == NULL || Filesystem_GetId(pMount->mountedFilesystem) != unmountingFsid) {
        throw(EINVAL);
    }


    // Can not unmount our root filesystem
    if (unmountingFsid == Filesystem_GetId(pManager->root->mountedFilesystem)) {
        throw(EINVAL);
    }


    // The error returned from OnUnmount is purely advisory but will not stop the unmount from completing
    err = Filesystem_OnUnmount(pMount->mountedFilesystem);

    Inode_SetMountpoint(pDirNode, false);
    List_Remove(&pManager->fileSystems, &pMount->node);

    Object_Release(pMount->mountedFilesystem);
    pMount->mountedFilesystem = NULL;
    Object_Release(pMount->mountingNode);
    pMount->mountingNode = NULL;
    kfree(pMount);

catch:
    Lock_Unlock(&pManager->lock);
    return err;
}

// This function should be called from a filesystem onUnmount() implementation
// to verify that the unmount can be completed safely. Eg no more open files
// exist that reference the filesystem. Note that the filesystem must do this
// call as part of its atomic unmount sequence. Eg the filesystem must lock its
// state, ensure that any operations that might have been ongoing have completed,
// then call this function before proceeding with the unmount.
Bool FilesystemManager_CanSafelyUnmountFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys)
{
    return ProcessManager_IsAnyProcessUsingFilesystem(gProcessManager, pFileSys);
}
