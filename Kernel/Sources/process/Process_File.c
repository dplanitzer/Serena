//
//  Process_Filesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FileChannel.h>


// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
FilePermissions Process_GetFileCreationMask(ProcessRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const FilePermissions mask = self->fileCreationMask;
    Lock_Unlock(&self->lock);
    return mask;
}

// Sets the file creation mask of the receiver.
void Process_SetFileCreationMask(ProcessRef _Nonnull self, FilePermissions mask)
{
    Lock_Lock(&self->lock);
    self->fileCreationMask = mask & 0777;
    Lock_Unlock(&self->lock);
}

// Creates a file in the given filesystem location.
errno_t Process_CreateFile(ProcessRef _Nonnull self, const char* _Nonnull path, unsigned int mode, FilePermissions permissions, int* _Nonnull pOutIoc)
{
    decl_try_err();
    ResolvedPath r;
    DirectoryEntryInsertionHint dih;
    InodeRef dir = NULL;
    InodeRef fileInode = NULL;
    IOChannelRef fileChannel = NULL;

    Lock_Lock(&self->lock);
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, path, self->rootDirectory, self->workingDirectory, self->realUser, &r));

    const PathComponent* pName = &r.lastPathComponent;
    FilesystemRef pFS = Inode_GetFilesystem(r.inode);
    dir = r.inode;
    r.inode = NULL;
    Inode_Lock(dir);


    // Can not create a file with names . or ..
    if ((pName->count == 1 && pName->name[0] == '.')
        || (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.')) {
        throw(EISDIR);
    }


    // The last path component must not exist
    err = Filesystem_AcquireNodeForName(pFS, dir, pName, self->realUser, &dih, &fileInode);
    if (err == EOK) {
        // File exists - reject the operation in exclusive mode and open the
        // file otherwise
        if ((mode & kOpen_Exclusive) == kOpen_Exclusive) {
            // Exclusive mode: File already exists -> throw an error
            throw(EEXIST);
        }
        else {
            Inode_Lock(fileInode);
            err = Filesystem_OpenFile(pFS, fileInode, mode, self->realUser);
            Inode_Unlock(fileInode);
            throw_iferr(err);
        }
    }
    else if (err == ENOENT) {
        // File does not exist - create it
        const FilePermissions filePerms = ~self->fileCreationMask & (permissions & 0777);


        // The user provided read/write mode must match up with the provided (user) permissions
        if ((mode & kOpen_ReadWrite) == 0) {
            throw(EACCESS);
        }
        if ((mode & kOpen_Read) == kOpen_Read && !FilePermissions_Has(filePerms, kFilePermissionsClass_User, kFilePermission_Read)) {
            throw(EACCESS);
        }
        if ((mode & kOpen_Write) == kOpen_Write && !FilePermissions_Has(filePerms, kFilePermissionsClass_User, kFilePermission_Write)) {
            throw(EACCESS);
        }


        // Create the new file and add it to its parent directory
        try(Filesystem_CreateNode(pFS, kFileType_RegularFile, self->realUser, filePerms, dir, pName, &dih, &fileInode));
    }
    else {
        throw(err);
    }

    Inode_UnlockRelinquish(dir);
    dir = NULL;


    // Note that the file channel takes ownership of the inode reference
    try(FileChannel_Create(fileInode, mode, &fileChannel));
    fileInode = NULL;
    try(IOChannelTable_AdoptChannel(&self->ioChannelTable, fileChannel, pOutIoc));
    fileChannel = NULL;

    ResolvedPath_Deinit(&r);
    Lock_Unlock(&self->lock);
    return EOK;

catch:
    Inode_UnlockRelinquish(dir);
    IOChannel_Release(fileChannel);
    Inode_Relinquish(fileInode);

    ResolvedPath_Deinit(&r);
    Lock_Unlock(&self->lock);
    
    *pOutIoc = -1;
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t Process_OpenFile(ProcessRef _Nonnull self, const char* _Nonnull path, unsigned int mode, int* _Nonnull pOutIoc)
{
    decl_try_err();
    ResolvedPath r;
    IOChannelRef chan = NULL;

    Lock_Lock(&self->lock);
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->realUser, &r));

    Inode_Lock(r.inode);
    err = Filesystem_OpenFile(Inode_GetFilesystem(r.inode), r.inode, mode, self->realUser);
    Inode_Unlock(r.inode);
    throw_iferr(err);

    // Note that this call takes ownership of the inode reference
    try(Filesystem_CreateChannel(Inode_GetFilesystem(r.inode), r.inode, mode, &chan));
    r.inode = NULL;
    try(IOChannelTable_AdoptChannel(&self->ioChannelTable, chan, pOutIoc));
    chan = NULL;

catch:
    ResolvedPath_Deinit(&r);
    Lock_Unlock(&self->lock);

    if (err != EOK) {
        IOChannel_Release(chan);
        *pOutIoc = -1;
    }

    return err;
}

// Returns information about the file at the given path.
errno_t Process_GetFileInfo(ProcessRef _Nonnull self, const char* _Nonnull path, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    ResolvedPath r;

    Lock_Lock(&self->lock);
    
    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->realUser, &r)) == EOK) {
        Inode_Lock(r.inode);
        err = Filesystem_GetFileInfo(Inode_GetFilesystem(r.inode), r.inode, pOutInfo);
        Inode_Unlock(r.inode);
    }

    ResolvedPath_Deinit(&r);
    Lock_Unlock(&self->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_GetFileInfoFromIOChannel(ProcessRef _Nonnull self, int ioc, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        if (instanceof(pChannel, FileChannel)) {
            err = FileChannel_GetInfo((FileChannelRef)pChannel, pOutInfo);
        }
        else if (instanceof(pChannel, DirectoryChannel)) {
            err = DirectoryChannel_GetInfo((DirectoryChannelRef)pChannel, pOutInfo);
        }
        else {
            err = EBADF;
        }

        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

// Modifies information about the file at the given path.
errno_t Process_SetFileInfo(ProcessRef _Nonnull self, const char* _Nonnull pPath, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    ResolvedPath r;

    Lock_Lock(&self->lock);
    
    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, pPath, self->rootDirectory, self->workingDirectory, self->realUser, &r)) == EOK) {
        Inode_Lock(r.inode);
        err = Filesystem_SetFileInfo(Inode_GetFilesystem(r.inode), r.inode, self->realUser, pInfo);
        Inode_Unlock(r.inode);
    }

    ResolvedPath_Deinit(&r);
    Lock_Unlock(&self->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_SetFileInfoFromIOChannel(ProcessRef _Nonnull self, int ioc, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        if (instanceof(pChannel, FileChannel)) {
            err = FileChannel_SetInfo((FileChannelRef)pChannel, self->realUser, pInfo);
        }
        else if (instanceof(pChannel, DirectoryChannel)) {
            err = DirectoryChannel_SetInfo((DirectoryChannelRef)pChannel, self->realUser, pInfo);
        }
        else {
            err = EBADF;
        }

        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }

    return err;
}

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
errno_t Process_TruncateFile(ProcessRef _Nonnull self, const char* _Nonnull path, FileOffset length)
{
    decl_try_err();
    ResolvedPath r;

    if (length < 0ll) {
        return EINVAL;
    }
    
    Lock_Lock(&self->lock);
    
    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->realUser, &r)) == EOK) {
        Inode_Lock(r.inode);
        if (Inode_IsRegularFile(r.inode)) {
            err = Filesystem_TruncateFile(Inode_GetFilesystem(r.inode), r.inode, self->realUser, length);
        }
        else {
            err = EISDIR;
        }
        Inode_Unlock(r.inode);
    }

    ResolvedPath_Deinit(&r);
    Lock_Unlock(&self->lock);

    return err;
}

// Same as above but the file is identified by the given I/O channel.
errno_t Process_TruncateFileFromIOChannel(ProcessRef _Nonnull self, int ioc, FileOffset length)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&self->ioChannelTable, ioc, &pChannel)) == EOK) {
        if (instanceof(pChannel, FileChannel)) {
            err = FileChannel_Truncate((FileChannelRef)pChannel, self->realUser, length);
        }
        else if (instanceof(pChannel, DirectoryChannel)) {
            err = EISDIR;
        }
        else {
            err = ENOTDIR;
        }

        IOChannelTable_RelinquishChannel(&self->ioChannelTable, pChannel);
    }
    return err;
}

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
errno_t Process_CheckAccess(ProcessRef _Nonnull self, const char* _Nonnull path, AccessMode mode)
{
    decl_try_err();
    ResolvedPath r;

    Lock_Lock(&self->lock);
    
    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->realUser, &r)) == EOK) {
        if (mode != kAccess_Exists) {
            Inode_Lock(r.inode);
            err = Filesystem_CheckAccess(Inode_GetFilesystem(r.inode), r.inode, self->realUser, mode);
            Inode_Unlock(r.inode);
        }
    }

    ResolvedPath_Deinit(&r);
    Lock_Unlock(&self->lock);

    return err;
}

// Unlinks the inode at the path 'pPath'.
errno_t Process_Unlink(ProcessRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    ResolvedPath r;
    InodeRef dir = NULL;
    InodeRef target = NULL;

    Lock_Lock(&self->lock);
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, path, self->rootDirectory, self->workingDirectory, self->realUser, &r));

    const PathComponent* pName = &r.lastPathComponent;
    dir = r.inode;
    r.inode = NULL;
    Inode_Lock(dir);


    // A path that ends in . or .. is not legal
    if ((pName->count == 1 && pName->name[0] == '.')
        || (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.')) {
        throw(EINVAL);
    }


    // Figure out what the target and parent node is
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(dir), dir, pName, self->realUser, NULL, &target));
    Inode_Lock(target);

    if (Inode_IsDirectory(target)) {
        // Can not unlink a mountpoint
        if (FileHierarchy_IsAttachmentPoint(self->fileHierarchy, target)) {
            throw(EBUSY);
        }


        // Can not unlink the root of a filesystem
        if (Inode_GetId(target) == Inode_GetId(dir)) {
            throw(EBUSY);
        }


        // Can not unlink the process' root directory
        if (Inode_Equals(self->rootDirectory, target)) {
            throw(EBUSY);
        }
    }

    try(Filesystem_Unlink(Inode_GetFilesystem(target), target, dir, self->realUser));

catch:
    Inode_UnlockRelinquish(target);
    Inode_UnlockRelinquish(dir);

    ResolvedPath_Deinit(&r);

    Lock_Unlock(&self->lock);
    return err;
}

static void ilock_ordered(InodeRef _Nonnull ip, InodeRef _Nonnull ipp[4], int* _Nonnull pCount)
{
    for(int i = 0; i < *pCount; i++) {
        if (Inode_Equals(ip, ipp[i])) {
            return;
        }
    }

    Inode_Lock(ip);
    ipp[*pCount] = ip;
    *pCount = *pCount + 1;
}

static void iunlock_ordered_all(InodeRef _Nonnull ipp[4], int* _Nonnull pCount)
{
    for (int i = *pCount - 1; i >= 0; i--) {
        Inode_Unlock(ipp[i]);
        ipp[i] = NULL;
    }
    *pCount = 0;
}

// Renames the file or directory at 'oldPath' to the new location 'newPath'.
errno_t Process_Rename(ProcessRef _Nonnull self, const char* oldPath, const char* newPath)
{
    decl_try_err();
    ResolvedPath or, nr;
    DirectoryEntryInsertionHint dih;
    InodeRef oldDir = NULL;
    InodeRef oldNode = NULL;
    InodeRef newDir = NULL;
    InodeRef newNode = NULL;
    InodeRef lockedNodes[4];
    int lockedNodeCount = 0;
    bool isMove = false;

    Lock_Lock(&self->lock);
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, oldPath, self->rootDirectory, self->workingDirectory, self->realUser, &or));
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, newPath, self->rootDirectory, self->workingDirectory, self->realUser, &nr));

    const PathComponent* pOldName = &or.lastPathComponent;
    const PathComponent* pNewName = &nr.lastPathComponent;


    // Final path components of . and .. are not supported
    if ((pOldName->count == 1 && pOldName->name[0] == '.')
        || (pOldName->count == 2 && pOldName->name[0] == '.' && pOldName->name[1] == '.')) {
        throw(EINVAL);
    }
    if ((pNewName->count == 1 && pNewName->name[0] == '.')
        || (pNewName->count == 2 && pNewName->name[0] == '.' && pNewName->name[1] == '.')) {
        throw(EINVAL);
    }


    // Lock the source and destination parents and figure out whether they are
    // the same
    oldDir = or.inode;
    or.inode = NULL;
    ilock_ordered(oldDir, lockedNodes, &lockedNodeCount);

    newDir = nr.inode;
    nr.inode = NULL;
    if (!Inode_Equals(oldDir, newDir)) {
        ilock_ordered(newDir, lockedNodes, &lockedNodeCount);
        isMove = true;
    }


    // newpath and oldpath have to be in the same filesystem
    if (Inode_GetFilesystem(oldDir) != Inode_GetFilesystem(newDir)) {
        throw(EXDEV);
    }


    // Get the source node. It must exist
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(oldDir), oldDir, pOldName, self->realUser, NULL, &oldNode));
    ilock_ordered(oldNode, lockedNodes, &lockedNodeCount);


    // The destination may exist
    err = Filesystem_AcquireNodeForName(Inode_GetFilesystem(newDir), newDir, pNewName, self->realUser, &dih, &newNode);
    if (err != EOK && err != ENOENT) {
        throw(err);
    }
    if (err == EOK) {
        if (Inode_Equals(oldNode, newNode)) {
            // Source and destination nodes are the same nodes -> do nothing
            err = EOK;
            goto catch;
        }

        ilock_ordered(newNode, lockedNodes, &lockedNodeCount);
    }


    // Source and destination nodes may not be mountpoints
    if (FileHierarchy_IsAttachmentPoint(self->fileHierarchy, oldNode)) {
        throw(EBUSY);
    }
    if (newNode != NULL && FileHierarchy_IsAttachmentPoint(self->fileHierarchy, newNode)) {
        throw(EBUSY);
    }


    // Make sure that the parent directories are writeable
    try(Filesystem_CheckAccess(Inode_GetFilesystem(oldDir), oldDir, self->realUser, kAccess_Writable));
    if (oldDir != newDir) {
        try(Filesystem_CheckAccess(Inode_GetFilesystem(newDir), newDir, self->realUser, kAccess_Writable));
    }


    // Remove the destination node if it exists
    if (newNode) {
        try(Filesystem_Unlink(Inode_GetFilesystem(newNode), newNode, newDir, self->realUser));
    }


    // Do the move or rename
    if (isMove) {
        err = Filesystem_Move(Inode_GetFilesystem(oldNode), oldNode, oldDir, newDir, pNewName, self->realUser, &dih);
    }
    else {
        err = Filesystem_Rename(Inode_GetFilesystem(oldNode), oldNode, oldDir, pNewName, self->realUser);
    }

catch:
    iunlock_ordered_all(lockedNodes, &lockedNodeCount);

    Inode_Relinquish(newNode);
    Inode_Relinquish(newDir);
    Inode_Relinquish(oldNode);
    Inode_Relinquish(oldDir);
    
    ResolvedPath_Deinit(&nr);
    ResolvedPath_Deinit(&or);
    
    Lock_Unlock(&self->lock);
    return err;
}
