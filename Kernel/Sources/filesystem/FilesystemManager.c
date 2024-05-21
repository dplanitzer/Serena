//
//  FilesystemManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/09/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "FilesystemManager.h"
#include <dispatcher/Lock.h>
#include <kobj/ObjectArray.h>


typedef struct Mountpoint {
    ListNode                node;
    
    // The FS that we are mounting...
    FilesystemRef _Nonnull  mountedFilesystem;

    // ...in this place
    FilesystemRef _Nullable mountingFilesystem;     // Only ever NULL for the root FS
    InodeRef _Nullable      mountingInode;          // Only ever NULL for the root FS
} Mountpoint;

typedef struct FilesystemManager {
    Lock                        lock;
    ObjectArray                 filesystems;
    List                        mountpoints;
    Mountpoint* _Weak _Nullable rootMountpoint;
} FilesystemManager;


FilesystemManagerRef    gFilesystemManager;


// Creates the filesystem manager.
errno_t FilesystemManager_Create(FilesystemManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    FilesystemManagerRef self;
    
    try(kalloc(sizeof(FilesystemManager), (void**) &self));
    Lock_Init(&self->lock);
    ObjectArray_Init(&self->filesystems, 4);
    List_Init(&self->mountpoints);
    self->rootMountpoint = NULL;

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

// Returns the mountpoint data structure for the given filesystem.
// Returns NULL if the filesystem isn't mounted.
static Mountpoint* _Nullable FilesystemManager_GetMountpointForFilesystem_Locked(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFS)
{
    const FilesystemId fsidToLookFor = Filesystem_GetId(pFS);

    List_ForEach(&self->mountpoints, Mountpoint, {
        if (fsidToLookFor == Filesystem_GetId(pCurNode->mountedFilesystem)) {
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
    pMount->mountingInode = (pDirNodeToMountAt) ? Inode_Reacquire(pDirNodeToMountAt) : NULL;

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

// Unmounts the given filesystem instance from the filesystem hierarchy.
static errno_t FilesystemManager_Unmount_Locked(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSysToUnmount, bool force)
{
    decl_try_err();

    // Make sure that 'pFileSysToUnmount' is actually mounted somewhere
    const FilesystemId unmountingFsid = Filesystem_GetId(pFileSysToUnmount);
    Mountpoint* pMount = FilesystemManager_GetMountpointForFilesystem_Locked(self, pFileSysToUnmount);
    if (pMount == NULL) {
        throw(EINVAL);
    }


    // All errors returned from unmount are purely informational except EBUSY.
    // The EBUSY error signals that the filesystem still has acquired inodes
    // outstanding (XXX in the future we'll allow force unmounting by removing the
    // FS from the file hierarchy but deferring the unmount until all inodes have
    // been relinquished)
    err = Filesystem_OnUnmount(pMount->mountedFilesystem);
    if (err == EBUSY) {
        throw(EBUSY);
    }

    List_Remove(&self->mountpoints, &pMount->node);
    if (List_IsEmpty(&self->mountpoints)) {
        self->rootMountpoint = NULL;
    }


    if (pMount->mountingInode) {
        Inode_SetMountpoint(pMount->mountingInode, false);
        err = Inode_Relinquish(pMount->mountingInode);
        pMount->mountingInode = NULL;
    }
    Object_Release(pMount->mountedFilesystem);
    pMount->mountedFilesystem = NULL;
    Object_Release(pMount->mountingFilesystem);
    pMount->mountingFilesystem = NULL;
    
    kfree(pMount);

catch:
    // XXX for now. If FS_unmount returns EBUSY -> out the FS in a deferred-unmount queue and attempt to unmount after we've terminated a process
    abort();
    return err;
}


// Returns a strong reference to the root of the global filesystem.
FilesystemRef _Nullable FilesystemManager_CopyRootFilesystem(FilesystemManagerRef _Nonnull self)
{
    FilesystemRef pFileSys;

    Lock_Lock(&self->lock);
    if (self->rootMountpoint) {
        pFileSys = Object_RetainAs(self->rootMountpoint->mountedFilesystem, Filesystem);
    }
    else {
        pFileSys = NULL;
    }
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

// Acquires the inode that is mounting the given filesystem instance. A suitable
// error and NULL is returned if the given filesystem is not mounted (anymore)
// or some other problem is detected.
// EOK and NULLs are returned if 'pFileSys' is the root filesystem (it has no
// parent file system).
errno_t FilesystemManager_AcquireNodeMountingFilesystem(FilesystemManagerRef _Nonnull pManager, FilesystemRef _Nonnull pFileSys, InodeRef _Nullable _Locked * _Nonnull pOutMountingNode)
{
    decl_try_err();

    Lock_Lock(&pManager->lock);
    const Mountpoint* pMount = FilesystemManager_GetMountpointForFilesystemId_Locked(pManager, Filesystem_GetId(pFileSys));
    if (pMount) {
        *pOutMountingNode = (pMount->mountingInode) ? Inode_Reacquire(pMount->mountingInode) : NULL;
        err = EOK;
    } else {
        *pOutMountingNode = NULL;
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
// filesystem instance may be mounted at at most one directory. If the node is
// NULL then the given filesystem is mounted as the root filesystem.
errno_t FilesystemManager_Mount(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSys, DiskDriverRef _Nonnull pDriver, const void* _Nullable pParams, ssize_t paramsSize, InodeRef _Nullable _Locked pDirNode)
{
    uint32_t dummyParam = 0;
    const void* ppParams = (pParams) ? pParams : &dummyParam;

    Lock_Lock(&self->lock);
    const errno_t err = FilesystemManager_Mount_Locked(self, pFileSys, pDriver, ppParams, paramsSize, pDirNode);
    Lock_Unlock(&self->lock);
    return err;
}

// Unmounts the given filesystem from the directory it is currently mounted on.
// Remember that one filesystem instance can be mounted at most once at any given
// time.
// A filesystem is only unmountable under normal circumstances if there are no
// more acquired inodes outstanding. Unmounting will fail with an EBUSY error if
// there is at least one acquired inode outstanding. However you may pass true
// for 'force' which forces the unmount. A forced unmount means that the
// filesystem will be immediately removed from the file hierarchy. However the
// unmounting and deallocation of the filesystem instance will be deferred until
// after the last outstanding inode has been relinquished.
errno_t FilesystemManager_Unmount(FilesystemManagerRef _Nonnull self, FilesystemRef _Nonnull pFileSys, bool force)
{
    Lock_Lock(&self->lock);
    const errno_t err = FilesystemManager_Unmount_Locked(self, pFileSys, force);
    Lock_Unlock(&self->lock);
    return err;
}
