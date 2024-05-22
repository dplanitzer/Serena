//
//  Filesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Filesystem.h"
#include <System/IOChannel.h>
#include <driver/MonotonicClock.h>


// Returns the next available FSID.
static FilesystemId Filesystem_GetNextAvailableId(void)
{
    // XXX want to:
    // XXX handle overflow (wrap around)
    // XXX make sure the generated id isn't actually in use by someone else
    static volatile AtomicInt gNextAvailableId = 0;
    return (FilesystemId) AtomicInt_Increment(&gNextAvailableId);
}

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
errno_t Filesystem_Create(Class* pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    FilesystemRef self;

    try(_Object_Create(pClass, 0, (ObjectRef*)&self));
    self->fsid = Filesystem_GetNextAvailableId();
    Lock_Init(&self->inodeManagementLock);
    PointerArray_Init(&self->inodesInUse, 16);

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void Filesystem_deinit(FilesystemRef _Nonnull self)
{
    PointerArray_Deinit(&self->inodesInUse);
    Lock_Deinit(&self->inodeManagementLock);
}

// Publishes the given inode. Publishing should be the last step in creating a
// new inode in order to make it visible and accessible to other part of the
// kernel. Note that the inode that is passed to this function must not have
// been acquired via Filesystem_AcquireNode(). The passed in inode must be a
// freshly allocated inode (via Inode_Create). The inode is considered acquired
// once this function returns. This means that you have to relinquish it by
// calling Filesystem_RelinquishNode(). 
errno_t Filesystem_PublishNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    decl_try_err();

    Lock_Lock(&self->inodeManagementLock);
    assert(pNode->useCount == 0);
    try(PointerArray_Add(&self->inodesInUse, pNode));
    pNode->useCount++;
    
catch:
    Lock_Unlock(&self->inodeManagementLock);
    return err;
}

// Acquires the inode with the ID 'id'. This methods guarantees that there will
// always only be at most one inode instance in memory at any given time and
// that only one VP can access/modify the inode.
// Once you're done with the inode, you should relinquish it back to the filesystem.
// This method should be used by subclassers to acquire inodes in order to return
// them to a filesystem user.
// This method calls the filesystem method onReadNodeFromDisk() to read the
// requested inode off the disk if there is no inode instance in memory at the
// time this method is called.
errno_t Filesystem_AcquireNodeWithId(FilesystemRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef pNode = NULL;

    Lock_Lock(&self->inodeManagementLock);

    for (int i = 0; i < PointerArray_GetCount(&self->inodesInUse); i++) {
        InodeRef pCurNode = (InodeRef)PointerArray_GetAt(&self->inodesInUse, i);

        if (Inode_GetId(pCurNode) == id) {
            pNode = pCurNode;
            break;
        }
    }

    if (pNode == NULL) {
        try(Filesystem_OnReadNodeFromDisk(self, id, &pNode));
        try(PointerArray_Add(&self->inodesInUse, pNode));
    }

    pNode->useCount++;
    Lock_Unlock(&self->inodeManagementLock);
    *pOutNode = pNode;

    return EOK;

catch:
    Lock_Unlock(&self->inodeManagementLock);
    *pOutNode = NULL;
    return err;
}

// Acquires a new reference to the given node. The returned node is locked.
InodeRef _Nonnull _Locked Filesystem_ReacquireNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    Lock_Lock(&self->inodeManagementLock);
    pNode->useCount++;
    Lock_Unlock(&self->inodeManagementLock);

    return pNode;
}

// Relinquishes the given node back to the filesystem. This method will invoke
// the filesystem onRemoveNodeFromDisk() if no directory is referencing the inode
// anymore. This will remove the inode from disk.
errno_t Filesystem_RelinquishNode(FilesystemRef _Nonnull self, InodeRef _Nullable _Locked pNode)
{
    decl_try_err();

    if (pNode == NULL) {
        return EOK;
    }
    
    Lock_Lock(&self->inodeManagementLock);

    // XXX take FS readonly status into account here
    // Remove the inode (file) from disk if the use count goes to 0 and the link
    // count is 0. The reason why we defer deleting the on-disk version of the
    // inode is that this allows you to create a file, immediately unlink it and
    // that you are then still able to read/write the file until you close it.
    // This gives you anonymous temporary storage that is guaranteed to disappear
    // with the death of the process.  
    if (pNode->useCount == 1 && pNode->linkCount == 0) {
        Filesystem_OnRemoveNodeFromDisk(self, pNode);
    }
    else if (Inode_IsModified(pNode)) {
        err = Filesystem_OnWriteNodeToDisk(self, pNode);
        Inode_ClearModified(pNode);
    }

    assert(pNode->useCount > 0);
    pNode->useCount--;
    if (pNode->useCount == 0) {
        PointerArray_Remove(&self->inodesInUse, pNode);
        Inode_Destroy(pNode);
    }

    Lock_Unlock(&self->inodeManagementLock);

    return err;
}

// Returns true if the filesystem can be unmounted which means that there are no
// acquired inodes outstanding that belong to this filesystem.
bool Filesystem_CanUnmount(FilesystemRef _Nonnull self)
{
    Lock_Lock(&self->inodeManagementLock);
    const bool ok = PointerArray_IsEmpty(&self->inodesInUse);
    Lock_Unlock(&self->inodeManagementLock);
    return ok;
}

// Invoked when Filesystem_AcquireNodeWithId() needs to read the requested inode
// off the disk. The override should read the inode data from the disk,
// create and inode instance and fill it in with the data from the disk and
// then return it. It should return a suitable error and NULL if the inode
// data can not be read off the disk.
errno_t Filesystem_onReadNodeFromDisk(FilesystemRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode)
{
    return EIO;
}

// Invoked when the inode is relinquished and it is marked as modified. The
// filesystem override should write the inode meta-data back to the 
// corresponding disk node.
errno_t Filesystem_onWriteNodeToDisk(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    return EIO;
}

// Invoked when Filesystem_RelinquishNode() has determined that the inode is
// no longer being referenced by any directory and that the on-disk
// representation should be deleted from the disk and deallocated. This
// operation is assumed to never fail.
void Filesystem_onRemoveNodeFromDisk(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
}


// Invoked when an instance of this file system is mounted. Note that the
// kernel guarantees that no operations will be issued to the filesystem
// before onMount() has returned with EOK.
errno_t Filesystem_onMount(FilesystemRef _Nonnull self, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize)
{
    return EIO;
}

// Invoked when a mounted instance of this file system is unmounted. A file
// system may return an error. Note however that this error is purely advisory
// and the file system implementation is required to do everything it can to
// successfully unmount. Unmount errors are ignored and the file system manager
// will complete the unmount in any case.
errno_t Filesystem_onUnmount(FilesystemRef _Nonnull self)
{
    return EOK;
}


// Returns the root node of the filesystem if the filesystem is currently in
// mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
errno_t Filesystem_acquireRootNode(FilesystemRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return EIO;
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function has to support the special name ".." (parent of node) in addition
// to "regular" filenames. If 'pParentNode' is the root node of the filesystem
// and 'pComponent' is ".." then 'pParentNode' should be returned. If the
// path component name is longer than what is supported by the file system,
// ENAMETOOLONG should be returned.  caller may pass a pointer to a
// directory-entry-insertion-hint data structure. This function may store
// information in this data structure to help speed up a follow up
// CreateNode() call for a node with the name 'pComponent' in the directory
// 'pParentNode'. You may pass NULL for 'pOutNode' which means that the
// function will do the inode lookup and return a status that reflects the
// outcome of the lookup, however the function will not return the looked up
// node. You can use this mechanism to check whether a directory contains a
// node with a name or not without forcing the creation of the node.
errno_t Filesystem_acquireNodeForName(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pComponent, User user, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    if (pOutNode) {
        *pOutNode = NULL;
    }
    
    if (pComponent->count == 2 && pComponent->name[0] == '.' && pComponent->name[1] == '.') {
        return EIO;
    } else {
        return ENOENT;
    }
}

// Returns the name of the node with the id 'id' which a child of the
// directory node 'pParentNode'. 'id' may be of any type. The name is
// returned in the mutable path component 'pComponent'. 'count' in path
// component is 0 on entry and should be set to the actual length of the
// name on exit. The function is expected to return EOK if the parent node
// contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
// file system is > the capacity of the path component, then ERANGE should
// be returned.
errno_t Filesystem_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent)
{
    pComponent->count = 0;
    return EIO;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
errno_t Filesystem_getFileInfo(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo)
{
    TimeInterval curTime;

    if (Inode_IsModified(pNode)) {
        curTime = MonotonicClock_GetCurrentTime();
    }

    if (Inode_IsAccessed(pNode)) {
        pOutInfo->accessTime = curTime;
    } else {
        pOutInfo->accessTime = Inode_GetAccessTime(pNode);
    }
    if (Inode_IsUpdated(pNode)) {
        pOutInfo->modificationTime = curTime;
    } else {
        pOutInfo->modificationTime = Inode_GetModificationTime(pNode);
    }
    if (Inode_IsStatusChanged(pNode)) {
        pOutInfo->statusChangeTime = curTime;
    } else {
        pOutInfo->statusChangeTime = Inode_GetStatusChangeTime(pNode);
    }
    
    pOutInfo->size = Inode_GetFileSize(pNode);
    pOutInfo->uid = Inode_GetUserId(pNode);
    pOutInfo->gid = Inode_GetGroupId(pNode);
    pOutInfo->permissions = Inode_GetFilePermissions(pNode);
    pOutInfo->type = Inode_GetFileType(pNode);
    pOutInfo->reserved = 0;
    pOutInfo->linkCount = Inode_GetLinkCount(pNode);
    pOutInfo->filesystemId = Filesystem_GetId(self);
    pOutInfo->inodeId = Inode_GetId(pNode);

    return EOK;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
errno_t Filesystem_setFileInfo(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo)
{
    const uint32_t  modify = pInfo->modify & kModifyFileInfo_All;

    if (modify == 0) {
        return EOK;
    }

    // Only the owner of a file may change its metadata.
    if (user.uid != Inode_GetUserId(pNode) && user.uid != kRootUserId) {
        return EPERM;
    }

    // Can't change the metadata if the disk is read-only
    if (!FilePermissions_Has(Filesystem_GetDiskPermissions(self, Inode_GetFileType(pNode)), kFilePermissionsClass_User, kFilePermission_Write)) {
        return EROFS;
    }
    

    // We got permissions. Now update the data as requested.
    if ((modify & kModifyFileInfo_UserId) == kModifyFileInfo_UserId) {
        Inode_SetUserId(pNode, pInfo->uid);
    }

    if ((modify & kModifyFileInfo_GroupId) == kModifyFileInfo_GroupId) {
        Inode_SetGroupId(pNode, pInfo->gid);
    }

    if ((modify & kModifyFileInfo_Permissions) == kModifyFileInfo_Permissions) {
        const FilePermissions oldPerms = Inode_GetFilePermissions(pNode);
        const FilePermissions newPerms = (oldPerms & ~pInfo->permissionsModifyMask) | (pInfo->permissions & pInfo->permissionsModifyMask);

        Inode_SetFilePermissions(pNode, newPerms);
    }

    // Update timestamps
    if ((modify & kModifyFileInfo_AccessTime) == kModifyFileInfo_AccessTime) {
        Inode_SetAccessTime(pNode, pInfo->accessTime);
    }
    if ((modify & kModifyFileInfo_ModificationTime) == kModifyFileInfo_ModificationTime) {
        Inode_SetModificationTime(pNode, pInfo->modificationTime);
    }

    Inode_SetModified(pNode, kInodeFlag_StatusChanged);

    return EOK;
}

// Creates a new inode with type 'type', user information 'user', permissions
// 'permissions' and adds it to parent inode (directory) 'pParentNode'. The new
// node will be added to 'pParentNode' with the name 'pName'. Returns the newly
// acquired inode on success and NULL otherwise.
errno_t Filesystem_createNode(FilesystemRef _Nonnull self, FileType type, User user, FilePermissions permissions, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pName, DirectoryEntryInsertionHint* _Nullable pDirInsertionHint, InodeRef _Nullable * _Nonnull pOutNode)
{
    return EIO;
}

// Opens the file identified by the given inode. The file is opened for reading
// and or writing, depending on the 'mode' bits.
errno_t Filesystem_openFile(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, unsigned int mode, User user)
{
    return EIO;
}

// Reads up to 'nBytesToRead' bytes starting at the file offset 'pInOutOffset'
// from the file 'pNode'.
errno_t Filesystem_readFile(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    return EIO;
}

// Writes up to 'nBytesToWrite' bytes starting at file offset 'pInOutOffset'
// to the file 'pNode'.
errno_t Filesystem_writeFile(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesWritten)
{
    return EIO;
}

// Opens the directory represented by the given node. The filesystem is
// expected to validate whether the user has access to the directory content.
errno_t Filesystem_openDirectory(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user)
{
    return EACCESS;
}

// Reads the next set of directory entries. The first entry read is the one
// at the current directory index stored in 'pDir'. This function guarantees
// that it will only ever return complete directories entries. It will never
// return a partial entry. Consequently the provided buffer must be big enough
// to hold at least one directory entry. Note that this function is expected
// to return "." for the entry at index #0 and ".." for the entry at index #1.
errno_t Filesystem_readDirectory(FilesystemRef _Nonnull self, InodeRef _Nonnull pDirNode, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    return EIO;
}

// Returns a set of file permissions that apply to all files of type 'fileType'
// on the disk. Ie if a filesystem supports a read-only mounting option then
// this function should return 0555. If the filesystem supports a do-not-
// execute-files mount option then this function should return 0666. A
// filesystem which always supports all permissions for all file types and
// permission classes should return 0777 (this is what the default
// implementation does).
FilePermissions Filesystem_getDiskPermissions(FilesystemRef _Nonnull self, FileType fileType)
{
    return FilePermissions_MakeFromOctal(0777);
}

// Verifies that the given node is accessible assuming the given access mode.
errno_t Filesystem_checkAccess(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, AccessMode mode)
{
    const FilePermissions diskPerms = Filesystem_GetDiskPermissions(self, Inode_GetFileType(pNode));
    const FilePermissions nodePerms = Inode_GetFilePermissions(pNode);
    FilePermissions reqPerms = 0;

    if ((mode & kAccess_Readable) == kAccess_Readable) {
        reqPerms |= kFilePermission_Read;
    }
    if ((mode & kAccess_Writable) == kAccess_Writable) {
        reqPerms |= kFilePermission_Write;
    }
    if ((mode & kAccess_Executable) == kAccess_Executable) {
        reqPerms |= kFilePermission_Execute;
    }


    FilePermissions finalDiskPerms;
    FilePermissions finalNodePerms;

    if (Inode_GetUserId(pNode) == user.uid) {
        finalDiskPerms = FilePermissions_Get(diskPerms, kFilePermissionsClass_User);
        finalNodePerms = FilePermissions_Get(nodePerms, kFilePermissionsClass_User);
    }
    else if (Inode_GetGroupId(pNode) == user.gid) {
        finalDiskPerms = FilePermissions_Get(diskPerms, kFilePermissionsClass_Group);
        finalNodePerms = FilePermissions_Get(nodePerms, kFilePermissionsClass_Group);
    }
    else {
        finalDiskPerms = FilePermissions_Get(diskPerms, kFilePermissionsClass_Other);
        finalNodePerms = FilePermissions_Get(nodePerms, kFilePermissionsClass_Other);
    }


    const FilePermissions finalPerms = finalNodePerms & finalDiskPerms;
    if ((finalPerms & reqPerms) == reqPerms) {
        return EOK;
    }


    // Return EROFS if the problem is that write permissions were requested but
    // the disk is read-only. Otherwise return EACCESS
    if ((mode & kAccess_Writable) == kAccess_Writable && (finalDiskPerms & kFilePermission_Write) == 0) {
        return EROFS;
    }
    else {
        return EACCESS;
    }
}

// Change the size of the file 'pNode' to 'length'. EINVAL is returned if
// the new length is negative. No longer needed blocks are deallocated if
// the new length is less than the old length and zero-fille blocks are
// allocated and assigned to the file if the new length is longer than the
// old length. Note that a filesystem implementation is free to defer the
// actual allocation of the new blocks until an attempt is made to read or
// write them.
errno_t Filesystem_truncate(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FileOffset length)
{
    return EIO;
}

// Unlink the node 'pNode' which is an immediate child of 'pParentDir'.
// Both nodes are guaranteed to be members of the same filesystem. 'pNode'
// is guaranteed to exist and that it isn't a mountpoint and not the root
// node of the filesystem.
// This function must validate that that if 'pNode' is a directory, that the
// directory is empty (contains nothing except "." and "..").
errno_t Filesystem_unlink(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeRef _Nonnull _Locked pParentDir, User user)
{
    return EACCESS;
}

// Changes the existing name of the node 'pSourceNode' which is an immediate
// child of the directory 'pSourceDir' such that it will be 'pNewName'.
errno_t Filesystem_rename(FilesystemRef _Nonnull self, InodeRef _Nonnull _Locked pSourceNode, InodeRef _Nonnull _Locked pSourceDir, const PathComponent* _Nonnull pNewName, User user)
{
    return EACCESS;
}


class_func_defs(Filesystem, Object,
override_func_def(deinit, Filesystem, Object)
func_def(onReadNodeFromDisk, Filesystem)
func_def(onWriteNodeToDisk, Filesystem)
func_def(onRemoveNodeFromDisk, Filesystem)
func_def(onMount, Filesystem)
func_def(onUnmount, Filesystem)
func_def(acquireRootNode, Filesystem)
func_def(acquireNodeForName, Filesystem)
func_def(getNameOfNode, Filesystem)
func_def(getFileInfo, Filesystem)
func_def(setFileInfo, Filesystem)
func_def(createNode, Filesystem)
func_def(openFile, Filesystem)
func_def(readFile, Filesystem)
func_def(writeFile, Filesystem)
func_def(openDirectory, Filesystem)
func_def(readDirectory, Filesystem)
func_def(getDiskPermissions, Filesystem)
func_def(checkAccess, Filesystem)
func_def(truncate, Filesystem)
func_def(unlink, Filesystem)
func_def(rename, Filesystem)
);
