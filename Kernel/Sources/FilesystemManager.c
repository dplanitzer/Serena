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
    ListNode        node;
    FilesystemRef   fileSystem;
    InodeRef        mountedAtNode;
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
    
    try_bang(kalloc(sizeof(FilesystemManager), (Byte**)&pManager));
    Lock_Init(&pManager->lock);
    List_Init(&pManager->fileSystems);
    try_bang(kalloc_cleared(sizeof(Mountpoint), (Byte**)&pManager->root));
    List_InsertAfter(&pManager->fileSystems, &pManager->root->node, NULL);
    pManager->root->fileSystem = Object_RetainAs(pRootFileSys, Filesystem);
    pManager->root->mountedAtNode = Filesystem_CopyRootNode(pRootFileSys);

    *pOutManager = pManager;
    return EOK;
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
    FilesystemRef pFileSys = NULL;

    Lock_Lock(&pManager->lock);
    List_ForEach(&pManager->fileSystems, Mountpoint, {
        FilesystemRef pCurFileSys = pCurNode->fileSystem;

        if (Filesystem_GetId(pCurFileSys) == fsid) {
            pFileSys = Object_RetainAs(pCurFileSys, Filesystem);
            break;
        }
    });
    Lock_Unlock(&pManager->lock);

    return pFileSys;
}

// Returns the node that mounts the filesystem with the given filesystem ID. Also
// returns the filesystem that owns the mounting node. ENOENT and NULLs are
// returned if the filesystem was never mounted or is no longer mounted.
ErrorCode _Nullable FilesystemManager_CopyNodeAndFilesystemMountingFilesystemId(FilesystemManagerRef _Nonnull pManager, FilesystemId fsid, InodeRef _Nullable * _Nonnull pOutNode, FilesystemRef _Nullable * _Nonnull pOutFilesystem)
{
    ErrorCode err = ENOENT;

    *pOutNode = NULL;
    *pOutFilesystem = NULL;

    Lock_Lock(&pManager->lock);
    List_ForEach(&pManager->fileSystems, Mountpoint, {
        if (Filesystem_GetId(pCurNode->fileSystem) == fsid) {
            *pOutNode = Object_RetainAs(pCurNode->mountedAtNode, Inode);
            *pOutFilesystem = Object_RetainAs(pCurNode->fileSystem, Filesystem);
            err = EOK;
            break;
        }
    });
    Lock_Unlock(&pManager->lock);

    return err;
}

// Mounts the given filesystem at the given node. The node must be a directory
// node. The same filesystem instance may be mounted at multiple different
// directories.
ErrorCode FilesystemManager_Mount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull pDirNode)
{
    decl_try_err();
    FilesystemId fsid = Filesystem_GetId(pFileSys);

    Lock_Lock(&pManager->lock);
    if (Inode_GetFilesystemId(pDirNode) == fsid) {
        throw(EINVAL);
    }
    
    List_ForEach(&pManager->fileSystems, Mountpoint, {
        FilesystemRef pCurFileSys = pCurNode->fileSystem;

        if (pCurFileSys == pFileSys) {
            throw(EINVAL);
        }
    });

    Mountpoint* pMount;
    throw(kalloc_cleared(sizeof(Mountpoint), (Byte**)&pMount));
    pMount->fileSystem = Object_RetainAs(pFileSys, Filesystem);
    pMount->mountedAtNode = Object_RetainAs(pDirNode, Inode);

    List_InsertAfterLast(&pManager->fileSystems, &pMount->node);
    Inode_SetMountedFilesystemId(pDirNode, fsid);

    Lock_Unlock(&pManager->lock);

    return EOK;

catch:
    Lock_Unlock(&pManager->lock);
    return err;
}

// Unmounts the given filesystem from the given directory.
void FilesystemManager_Unmount(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull pDirNode)
{
    Lock_Lock(&pManager->lock);
    assert(pFileSys != pManager->root->fileSystem);

    Mountpoint* pMount = NULL;
    List_ForEach(&pManager->fileSystems, Mountpoint, {
        if (pCurNode->fileSystem == pFileSys) {
            pMount = pCurNode;
            break;
        }
    });

    if (pMount) {
        Inode_SetMountedFilesystemId(pDirNode, 0);

        List_Remove(&pManager->fileSystems, &pMount->node);

        Object_Release(pMount->fileSystem);
        pMount->fileSystem = NULL;
        Object_Release(pMount->mountedAtNode);
        pMount->mountedAtNode = NULL;
        kfree((Byte*)pMount);
    }

    Lock_Unlock(&pManager->lock);
}
