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
    InodeRef _Nonnull       mountedInode;

    // ...in this place
    FilesystemRef _Nullable mountingFilesystem;     // Only ever NULL for the root FS
    InodeRef _Nullable      mountingInode;          // Only ever NULL for the root FS
} Mountpoint;

typedef struct _FilesystemManager {
    Lock                    lock;
    List                    fileSystems;
    Mountpoint* _Nonnull    root;
} FilesystemManager;


FilesystemManagerRef    gFilesystemManager;


// Creates the filesystem manager. The provided filesystem is automatically
// mounted as the root filesystem.
ErrorCode FilesystemManager_Create(FilesystemRef _Nonnull pRootFileSys, FilesystemManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    FilesystemManagerRef pManager;
    
    try(kalloc(sizeof(FilesystemManager), (void**) &pManager));
    Lock_Init(&pManager->lock);
    List_Init(&pManager->fileSystems);

    try(kalloc_cleared(sizeof(Mountpoint), (void**) &pManager->root));

    Byte dummy;
    InodeRef pRootNode;
    try(Filesystem_OnMount(pRootFileSys, &dummy, 0, &pRootNode));

    pManager->root->mountedFilesystem = Object_RetainAs(pRootFileSys, Filesystem);
    pManager->root->mountedInode = pRootNode;
    pManager->root->mountingFilesystem = NULL;
    pManager->root->mountingInode = NULL;

    List_InsertAfter(&pManager->fileSystems, &pManager->root->node, NULL);

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
        if (Filesystem_GetId(pCurNode->mountedFilesystem) == fsid) {
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
        if (Inode_Equals(pNode, pCurNode->mountingInode)) {
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
    return Object_RetainAs(pManager->root->mountedInode, Inode);
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

// Returns the mountpoint information of the node and filesystem that mounts the
// given filesystem. ENOENT and NULLs are returned if the filesystem was never
// mounted or is no longer mounted. EOK and NULLs are returned if 'pFileSys' is
// the root filesystem (it has no parent file system).
ErrorCode FilesystemManager_CopyMountpointOfFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nullable * _Nonnull pOutMountingNode, FilesystemRef _Nullable * _Nonnull pOutMountingFilesystem)
{
    Lock_Lock(&pManager->lock);
    const Mountpoint* pMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, Filesystem_GetId(pFileSys));
    ErrorCode err;

    if (pMount) {
        *pOutMountingNode = (pMount->mountingInode) ? Object_RetainAs(pMount->mountingInode, Inode) : NULL;
        *pOutMountingFilesystem = (pMount->mountingInode) ? Object_RetainAs(pMount->mountingFilesystem, Filesystem) : NULL;
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
    Inode_Lock(pNode);
    const Bool isMountpoint = Inode_IsMountpoint(pNode);

    if (isMountpoint) {
        const Mountpoint* pMountpoint = FilesystemManager_GetMountpointForInode_Locked(pManager, pNode);

        *pMountedFilesystem = Object_RetainAs(pMountpoint->mountedFilesystem, Filesystem);
        *pMountedRootNode = Object_RetainAs(pMountpoint->mountedInode, Inode);
    } else {
        *pMountedFilesystem = NULL;
        *pMountedRootNode = NULL;
    }

    Inode_Unlock(pNode);
    Lock_Unlock(&pManager->lock);
    
    return isMountpoint;
}

// Mounts the given filesystem at the given node. The node must be a directory
// node. A filesystem instance may be mounted at at most one directory.
ErrorCode FilesystemManager_Mount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, const Byte* _Nonnull pParams, ByteCount paramsSize, InodeRef _Nonnull pDirNode)
{
    decl_try_err();

    Lock_Lock(&pManager->lock);
    Inode_Lock(pDirNode);

    // Make sure that 'pDirNode' isn't owned by the filesystem we want to mount
    // and that the filesystem instance we want to mount isn't already mounted
    // somewhere else 
    const FilesystemId mountingFsid = Filesystem_GetId(pFileSys);
    const FilesystemId mountedOnFsid = Inode_GetFilesystemId(pDirNode);
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


    Mountpoint* pMount;
    throw(kalloc_cleared(sizeof(Mountpoint), (void**) &pMount));


    // Notify the filesystem that we are mounting it
    InodeRef pRootNode;
    try(Filesystem_OnMount(pFileSys, pParams, paramsSize, &pRootNode));


    // Update our mount table
    pMount->mountedFilesystem = Object_RetainAs(pFileSys, Filesystem);
    pMount->mountedInode = pRootNode;
    pMount->mountingFilesystem = Object_RetainAs(pDirNodeMount->mountedFilesystem, Filesystem);
    pMount->mountingInode = Object_RetainAs(pDirNode, Inode);

    List_InsertAfterLast(&pManager->fileSystems, &pMount->node);
    Inode_SetMountpoint(pDirNode, true);

catch:
    Inode_Unlock(pDirNode);
    Lock_Unlock(&pManager->lock);
    return err;
}

// Unmounts the given filesystem from the given directory.
ErrorCode FilesystemManager_Unmount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull pDirNode)
{
    decl_try_err();

    Lock_Lock(&pManager->lock);
    Inode_Lock(pDirNode);


    // Make sure that 'pFileSys' is actually mounted at 'pDirNode'
    const FilesystemId unmountingFsid = Filesystem_GetId(pFileSys);
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
    Object_Release(pMount->mountedInode);
    pMount->mountedInode = NULL;
    Object_Release(pMount->mountingFilesystem);
    pMount->mountingFilesystem = NULL;
    Object_Release(pMount->mountingInode);
    pMount->mountingInode = NULL;
    kfree(pMount);

catch:
    Inode_Unlock(pDirNode);
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
