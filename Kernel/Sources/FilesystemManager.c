//
//  FilesystemManager.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/09/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include "Lock.h"


typedef struct _Mountpoint {
    ListNode                node;
    FilesystemRef _Nonnull  fileSystem;
    InodeRef _Nullable      mountedAtNode;  // Only ever NULL for the root FS
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
    pManager->root->fileSystem = Object_RetainAs(pRootFileSys, Filesystem);
    pManager->root->mountedAtNode = NULL;
    
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
static Mountpoint* _Nullable _Nullable FilesystemManager_GetMountpointForFilesystemId_Locked(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid)
{
    List_ForEach(&pManager->fileSystems, Mountpoint, {
        FilesystemRef pCurFileSys = pCurNode->fileSystem;

        if (Filesystem_GetId(pCurFileSys) == fsid) {
            return pCurNode;
        }
    });

    return NULL;
}

// Returns a strong reference to the root of the global filesystem.
FilesystemRef _Nonnull FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull pManager)
{
    // rootFileSys is a constant value, so no locking needed
    return Object_RetainAs(pManager->root->fileSystem, Filesystem);
}

// Returns the node that represents the root of the global filesystem.
InodeRef _Nonnull FilesystemManager_CopyRootNode(FilesystemManagerRef _Nonnull pManager)
{
    // rootNode is a constant value, so no locking needed
    return Object_RetainAs(pManager->root->mountedAtNode, Inode);
}

// Returns the filesystem for the given filesystem ID. NULL is returned if no
// file system for the given ID is registered/mounted anywhere in the global
// namespace.
FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid)
{
    Lock_Lock(&pManager->lock);
    const Mountpoint* pMountpoint = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, fsid);
    const FilesystemRef pFileSys = (pMountpoint) ? Object_RetainAs(pMountpoint->fileSystem, Filesystem) : NULL;
    Lock_Unlock(&pManager->lock);

    return pFileSys;
}

// Returns the node that mounts the filesystem with the given filesystem ID. Also
// returns the filesystem that owns the mounting node. ENOENT and NULLs are
// returned if the filesystem was never mounted or is no longer mounted. EOK and
// NULLs are returned if 'fsid' is the root filesystem (it has no parent file
// system).
ErrorCode _Nullable FilesystemManager_CopyNodeAndFilesystemMountingFilesystemId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid, InodeRef _Nullable * _Nonnull pOutNode, FilesystemRef _Nullable * _Nonnull pOutFilesystem)
{
    Lock_Lock(&pManager->lock);
    const Mountpoint* pMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, fsid);
    ErrorCode err;

    if (pMount) {
        *pOutNode = (pMount->mountedAtNode) ? Object_RetainAs(pMount->mountedAtNode, Inode) : NULL;
        *pOutFilesystem = (pMount->mountedAtNode) ? Object_RetainAs(FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, Inode_GetFilesystemId(pMount->mountedAtNode))->fileSystem, Filesystem) : NULL;
        err = EOK;
    } else {
        *pOutNode = NULL;
        *pOutFilesystem = NULL;
        err = ENOENT;
    }
    Lock_Unlock(&pManager->lock);

    return err;
}

// Mounts the given filesystem at the given node. The node must be a directory
// node. The same filesystem instance may be mounted at multiple different
// directories.
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
    pMount->fileSystem = Object_RetainAs(pFileSys, Filesystem);
    pMount->mountedAtNode = Object_RetainAs(pDirNode, Inode);

    List_InsertAfterLast(&pManager->fileSystems, &pMount->node);
    Filesystem_SetFilesystemMountedOnNode(pDirNodeMount->fileSystem, pDirNode, mountingFsid);

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
    const FilesystemId mountedOnFsid = Inode_GetFilesystemId(pDirNode);

    // Can not unmount our root filesystem
    if (unmountingFsid == Filesystem_GetId(pManager->root->fileSystem)) {
        throw(EINVAL);
    }


    // Get the mount point of the FS to unmount and of the FS that owns 'pDirNode'
    Mountpoint* pMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, unmountingFsid);
    const Mountpoint* pDirNodeMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, mountedOnFsid);

    if (pMount) {
        if (pDirNodeMount) {
            Filesystem_SetFilesystemMountedOnNode(pDirNodeMount->fileSystem, pDirNode, 0);
        }

        List_Remove(&pManager->fileSystems, &pMount->node);

        Object_Release(pMount->fileSystem);
        pMount->fileSystem = NULL;
        Object_Release(pMount->mountedAtNode);
        pMount->mountedAtNode = NULL;
        kfree(pMount);
    }

    Lock_Unlock(&pManager->lock);
    return EOK;

catch:
    Lock_Unlock(&pManager->lock);
    return err;
}
