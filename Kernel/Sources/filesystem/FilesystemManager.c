//
//  FilesystemManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/09/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include "Lock.h"


typedef struct _Mountpoint {
    ListNode                node;
    
    // The FS that we are mounting...
    FilesystemRef _Nonnull  mountedFilesystem;

    // ...in this place
    FilesystemRef _Nullable mountingFilesystem;     // Only ever NULL for the root FS
    InodeRef _Nullable      mountingInode;          // Only ever NULL for the root FS
} Mountpoint;

typedef struct _FilesystemManager {
    Lock                    lock;
    ObjectArray             filesystems;
    List                    mountpoints;
    Mountpoint*             rootMountpoint;
} FilesystemManager;

static errno_t FilesystemManager_Mount_Locked(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSysToMount, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize, InodeRef _Nullable pDirNodeToMountAt);


FilesystemManagerRef    gFilesystemManager;


// Creates the filesystem manager. The provided filesystem is automatically
// mounted as the root filesystem on the disk partition 'pDriver'.
errno_t FilesystemManager_Create(FilesystemRef _Nonnull pRootFileSys, DiskDriverRef _Nonnull pDriver, FilesystemManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    FilesystemManagerRef self;
    
    try(kalloc(sizeof(FilesystemManager), (void**) &self));
    Lock_Init(&self->lock);
    ObjectArray_Init(&self->filesystems, 4);
    List_Init(&self->mountpoints);
    self->rootMountpoint = NULL;

    char dummy;
    try(FilesystemManager_Mount_Locked(self, pRootFileSys, pDriver, &dummy, 0, NULL));

    *pOutManager = self;
    return EOK;

catch:
    *pOutManager = NULL;
    return err;
}

// Returns a weak reference to the filesystem for the given fsid. NULL is
// returned if no filesystem with this fsid is registered.
static FilesystemRef _Nullable FilesystemManager_GetFilesystemForId_Locked(FilesystemManagerRef _Nonnull self, FilesystemId fsid)
{
    for (int i = 0; i < ObjectArray_GetCount(&self->filesystems); i++) {
        FilesystemRef pFileSys = (FilesystemRef)ObjectArray_GetAt(&self->filesystems, i);

        if (Filesystem_GetId(pFileSys) == fsid) {
            return pFileSys;
        }
    }

    return NULL;
}

// Registers the given filesystem if it isn't already registered.
static void FilesystemManager_RegisterFilesystem_Locked(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSys)
{
    for (int i = 0; i < ObjectArray_GetCount(&self->filesystems); i++) {
        if ((FilesystemRef)ObjectArray_GetAt(&self->filesystems, i) == pFileSys) {
            return;
        }
    }

    ObjectArray_Add(&self->filesystems, (ObjectRef)pFileSys);
}

// Unregisters the given filesystem.
static void FilesystemManager_UnregisterFilesystem_Locked(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSys)
{
    ObjectArray_RemoveIdenticalTo(&self->filesystems, (ObjectRef)pFileSys);
}

// Returns the mountpoint data structure for the given filesystem ID. NULL is
// returned if the given filesystem is not mounted.
static Mountpoint* _Nullable FilesystemManager_GetMountpointForFilesystemId_Locked(FilesystemManagerRef _Nonnull self, FilesystemId fsid)
{
    List_ForEach(&self->mountpoints, Mountpoint, {
        if (Filesystem_GetId(pCurNode->mountedFilesystem) == fsid) {
            return pCurNode;
        }
    });

    return NULL;
}

// Returns the mountpoint data structure for the given node if it is a mountpoint.
// Returns NULL if it is not.
static Mountpoint* _Nullable FilesystemManager_GetMountpointForInode_Locked(FilesystemManagerRef _Nonnull self, InodeRef _Nonnull pNode)
{
    List_ForEach(&self->mountpoints, Mountpoint, {
        if (Inode_Equals(pNode, pCurNode->mountingInode)) {
            return pCurNode;
        }
    });

    return NULL;
}

// Internal mount function. Mounts the given filesystem at the given place. If
// 'pDirNodeToMountAt' is NULL then 'pFileSysToMount' is mounted as the root
// filesystem.
static errno_t FilesystemManager_Mount_Locked(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSysToMount, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize, InodeRef _Nullable _Locked pDirNodeToMountAt)
{
    const Mountpoint* pDirNodeMount = NULL;
    decl_try_err();

    if (pDirNodeToMountAt) {
        // Make sure that 'pDirNodeToMountAt' isn't owned by the filesystem we want
        // to mount and that the filesystem instance we want to mount isn't already
        // mounted somewhere else 
        const FilesystemId mountingFsid = Filesystem_GetId(pFileSysToMount);
        const FilesystemId mountedOnFsid = Inode_GetFilesystemId(pDirNodeToMountAt);
        if (mountedOnFsid == mountingFsid) {
            throw(EINVAL);
        }
    
        if (FilesystemManager_GetMountpointForFilesystemId_Locked(self, mountingFsid)) {
            throw(EINVAL);
        }


        // Make sure that filesystem that owns 'pDirNodeToMountAt' is still mounted and get it.
        pDirNodeMount = FilesystemManager_GetMountpointForFilesystemId_Locked(self, mountedOnFsid);
        if (pDirNodeMount == NULL) {
            throw(EINVAL);
        }
    }


    Mountpoint* pMount;
    try(kalloc_cleared(sizeof(Mountpoint), (void**) &pMount));


    // Notify the filesystem that we are mounting it
    try(Filesystem_OnMount(pFileSysToMount, pDriver, pParams, paramsSize));


    // Update our mount table
    pMount->mountedFilesystem = Object_RetainAs(pFileSysToMount, Filesystem);
    pMount->mountingFilesystem = (pDirNodeToMountAt) ? Object_RetainAs(pDirNodeMount->mountedFilesystem, Filesystem) : NULL;
    pMount->mountingInode = (pDirNodeToMountAt) ? Filesystem_ReacquireUnlockedNode(pMount->mountingFilesystem, pDirNodeToMountAt) : NULL;

    if (List_IsEmpty(&self->mountpoints)) {
        self->rootMountpoint = pMount;
    }
    List_InsertAfterLast(&self->mountpoints, &pMount->node);
    FilesystemManager_RegisterFilesystem_Locked(self, pFileSysToMount);

    if (pDirNodeToMountAt) {
        Inode_SetMountpoint(pDirNodeToMountAt, true);
    }

catch:
    return err;
}

// Unmounts the given filesystem from the given directory.
static errno_t FilesystemManager_Unmount_Locked(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSysToUnmount, InodeRef _Nonnull _Locked pDirNode)
{
    decl_try_err();

    // Make sure that 'pFileSys' is actually mounted at 'pDirNode'
    const FilesystemId unmountingFsid = Filesystem_GetId(pFileSysToUnmount);
    Mountpoint* pMount = NULL;
    
    if (Inode_IsMountpoint(pDirNode)) {
        pMount = FilesystemManager_GetMountpointForInode_Locked(self, pDirNode);
    }
    if (pMount == NULL || Filesystem_GetId(pMount->mountedFilesystem) != unmountingFsid) {
        throw(EINVAL);
    }


    // Can not unmount our root filesystem
    if (unmountingFsid == Filesystem_GetId(self->rootMountpoint->mountedFilesystem)) {
        throw(EINVAL);
    }


    // The error returned from OnUnmount is purely advisory but will not stop the unmount from completing
    err = Filesystem_OnUnmount(pMount->mountedFilesystem);

    Inode_SetMountpoint(pDirNode, false);
    List_Remove(&self->mountpoints, &pMount->node);

    Object_Release(pMount->mountedFilesystem);
    pMount->mountedFilesystem = NULL;
    if (pMount->mountingInode) {
        Filesystem_RelinquishNode(pMount->mountingFilesystem, pMount->mountingInode);
        pMount->mountingInode = NULL;
    }
    Object_Release(pMount->mountingFilesystem);
    pMount->mountingFilesystem = NULL;
    kfree(pMount);

catch:
    return err;
}


// Returns a strong reference to the root of the global filesystem.
FilesystemRef _Nonnull FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    FilesystemRef pFileSys = Object_RetainAs(self->rootMountpoint->mountedFilesystem, Filesystem);
    Lock_Unlock(&self->lock);

    return pFileSys;
}

// Returns the filesystem for the given filesystem ID. NULL is returned if no
// file system for the given ID is registered/mounted anywhere in the global
// namespace.
FilesystemRef _Nullable FilesystemManager_CopyFilesystemForId(FilesystemManagerRef _Nonnull self, FilesystemId fsid)
{
    Lock_Lock(&self->lock);
    const FilesystemRef pFileSysWeakRef = FilesystemManager_GetFilesystemForId_Locked(self, fsid);
    const FilesystemRef pFileSys = (pFileSysWeakRef) ? Object_RetainAs(pFileSysWeakRef, Filesystem) : NULL;
    Lock_Unlock(&self->lock);

    return pFileSys;
}

// Returns the mountpoint information of the node and filesystem that mounts the
// given filesystem. ENOENT and NULLs are returned if the filesystem was never
// mounted or is no longer mounted. EOK and NULLs are returned if 'pFileSys' is
// the root filesystem (it has no parent file system).
errno_t FilesystemManager_CopyMountpointOfFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nullable _Locked * _Nonnull pOutMountingNode, FilesystemRef _Nullable * _Nonnull pOutMountingFilesystem)
{
    Lock_Lock(&pManager->lock);
    const Mountpoint* pMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, Filesystem_GetId(pFileSys));
    errno_t err;

    if (pMount) {
        *pOutMountingNode = (pMount->mountingInode) ? Filesystem_ReacquireNode(pMount->mountingFilesystem, pMount->mountingInode) : NULL;
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

// Returns true if the given node is a mountpoint and false otherwise.
bool FilesystemManager_IsNodeMountpoint(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull _Locked pNode)
{
    Lock_Lock(&pManager->lock);
    const bool r = Inode_IsMountpoint(pNode);
    Lock_Unlock(&pManager->lock);
    
    return r;
}

// Checks whether the given node is a mount point and returns the filesystem
// mounted at that node, if it is. Otherwise returns NULL.
FilesystemRef _Nullable FilesystemManager_CopyFilesystemMountedAtNode(FilesystemManagerRef _Nonnull pManager, InodeRef _Nonnull _Locked pNode)
{
    FilesystemRef pFileSys = NULL;

    Lock_Lock(&pManager->lock);

    if (Inode_IsMountpoint(pNode)) {
        const Mountpoint* pMount = FilesystemManager_GetMountpointForInode_Locked(pManager, pNode);
        
        assert(pMount != NULL);
        pFileSys = Object_RetainAs(pMount->mountedFilesystem, Filesystem);
    }

    Lock_Unlock(&pManager->lock);
    
    return pFileSys;
}

// Mounts the given filesystem physically located at the given disk partition
// and attaches it at the given node. The node must be a directory node. A
// filesystem instance may be mounted at at most one directory.
errno_t FilesystemManager_Mount(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSys, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize, InodeRef _Nonnull _Locked pDirNode)
{
    Lock_Lock(&self->lock);
    const errno_t err = FilesystemManager_Mount_Locked(self, pFileSys, pDriver, pParams, paramsSize, pDirNode);
    Lock_Unlock(&self->lock);
    return err;
}

// Unmounts the given filesystem from the given directory.
errno_t FilesystemManager_Unmount(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSys, InodeRef _Nonnull _Locked pDirNode)
{
    Lock_Lock(&self->lock);
    const errno_t err = FilesystemManager_Unmount_Locked(self, pFileSys, pDirNode);
    Lock_Unlock(&self->lock);
    return err;
}
