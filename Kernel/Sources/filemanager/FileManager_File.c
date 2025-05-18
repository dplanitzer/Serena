//
//  FileManager_File.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/16/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FileManager.h"
#include "FileHierarchy.h"
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FileChannel.h>
#include <security/SecurityManager.h>
#include <kpi/fcntl.h>
#include <kpi/perm.h>


errno_t _FileManager_OpenFile(FileManagerRef _Nonnull self, InodeRef _Nonnull _Locked pFile, unsigned int mode)
{
    decl_try_err();
    FilesystemRef fs = Inode_GetFilesystem(pFile);
    int accessMode = 0;


    // This must be some kind of file and not a directory
    if (Inode_IsDirectory(pFile)) {
        return EISDIR;
    }


    // Calculate the desired access mode
    if ((mode & O_RDWR) == 0) {
        return EACCESS;
    }
    if ((mode & O_RDONLY) == O_RDONLY) {
        accessMode |= R_OK;
    }
    if ((mode & O_WRONLY) == O_WRONLY || (mode & O_TRUNC) == O_TRUNC) {
        accessMode |= W_OK;
    }


    // Check access mode, validate the file size and truncate the file if
    // requested
    err = SecurityManager_CheckNodeAccess(gSecurityManager, pFile, self->ruid, self->rgid, accessMode);
    if (err == EOK) {
        if (Inode_GetFileSize(pFile) >= 0ll) {
            if ((mode & O_TRUNC) == O_TRUNC) {
                err = Inode_Truncate(pFile, 0);
            }
        }
        else {
            // A negative file size is treated as an overflow
            err = EOVERFLOW;
        }
    }
    
    return err;
}

// Creates a file in the given filesystem location.
errno_t FileManager_CreateFile(FileManagerRef _Nonnull self, const char* _Nonnull path, unsigned int mode, mode_t permissions, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath r;
    DirectoryEntryInsertionHint dih;
    InodeRef dir = NULL;
    InodeRef ip = NULL;

    *pOutChannel = NULL;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r));

    const PathComponent* name = &r.lastPathComponent;
    FilesystemRef pFS = Inode_GetFilesystem(r.inode);
    dir = r.inode;
    r.inode = NULL;
    Inode_Lock(dir);


    // Can not create a file with names . or ..
    if ((name->count == 1 && name->name[0] == '.')
        || (name->count == 2 && name->name[0] == '.' && name->name[1] == '.')) {
        throw(EISDIR);
    }


    // The last path component must not exist
    err = Filesystem_AcquireNodeForName(pFS, dir, name, &dih, &ip);
    if (err == EOK) {
        // File exists - reject the operation in exclusive mode and open the
        // file otherwise
        if ((mode & O_EXCL) == O_EXCL) {
            // Exclusive mode: File already exists -> throw an error
            throw(EEXIST);
        }
        else {
            Inode_Lock(ip);
            err = _FileManager_OpenFile(self, ip, mode);
            Inode_Unlock(ip);
            throw_iferr(err);
        }
    }
    else if (err == ENOENT) {
        // File does not exist - create it
        const mode_t filePerms = ~self->umask & (permissions & 0777);


        // The user provided read/write mode must match up with the provided (user) permissions
        if ((mode & O_RDWR) == 0) {
            throw(EACCESS);
        }
        if ((mode & O_RDONLY) == O_RDONLY && !perm_has(filePerms, S_ICUSR, S_IR)) {
            throw(EACCESS);
        }
        if ((mode & O_WRONLY) == O_WRONLY && !perm_has(filePerms, S_ICUSR, S_IW)) {
            throw(EACCESS);
        }


        // We must have write permissions for the parent directory
        try(SecurityManager_CheckNodeAccess(gSecurityManager, dir, self->ruid, self->rgid, W_OK));


        // Create the new file and add it to its parent directory
        try(Filesystem_CreateNode(pFS, S_IFREG, dir, name, &dih, self->ruid, self->rgid, filePerms, &ip));
    }
    else {
        throw(err);
    }


    // Create the file channel
    try(FileChannel_Create(ip, mode, pOutChannel));

catch:
    Inode_Relinquish(ip);
    Inode_UnlockRelinquish(dir);

    ResolvedPath_Deinit(&r);
    
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t FileManager_OpenFile(FileManagerRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath r;

    *pOutChannel = NULL;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r));

    Inode_Lock(r.inode);
    err = _FileManager_OpenFile(self, r.inode, mode);
    Inode_Unlock(r.inode);
    throw_iferr(err);

    err = Inode_CreateChannel(r.inode, mode, pOutChannel);

catch:
    ResolvedPath_Deinit(&r);
    return err;
}

// Opens an executable file.
errno_t FileManager_OpenExecutable(FileManagerRef _Nonnull self, const char* _Nonnull path, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    decl_try_err();
    ResolvedPath r;

    *pOutChannel = NULL;

    // Resolve the path to the executable file
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r));


    // Make sure that the executable is a regular file and that it has the
    // correct access mode
    Inode_Lock(r.inode); 
    if (Inode_IsRegularFile(r.inode)) {
        err = SecurityManager_CheckNodeAccess(gSecurityManager, r.inode, self->ruid, self->rgid, R_OK | X_OK);
        if (err == EOK && Inode_GetFileSize(r.inode) < 0ll) {
            // Negative file size means that the file size overflowed
            err = E2BIG;
        }
    }
    else {
        err = EACCESS;
    }
    Inode_Unlock(r.inode);
    throw_iferr(err);

    err = Inode_CreateChannel(r.inode, O_RDONLY, pOutChannel);

catch:
    ResolvedPath_Deinit(&r);
    return err;
}

// Returns information about the file at the given path.
errno_t FileManager_GetFileInfo(FileManagerRef _Nonnull self, const char* _Nonnull path, finfo_t* _Nonnull pOutInfo)
{
    decl_try_err();
    ResolvedPath r;

    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r)) == EOK) {
        Inode_Lock(r.inode);
        err = Inode_GetInfo(r.inode, pOutInfo);
        Inode_Unlock(r.inode);
    }

    ResolvedPath_Deinit(&r);
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t FileManager_GetFileInfo_ioc(FileManagerRef _Nonnull self, IOChannelRef _Nonnull pChannel, finfo_t* _Nonnull pOutInfo)
{
    decl_try_err();

    if (instanceof(pChannel, FileChannel)) {
        err = FileChannel_GetInfo((FileChannelRef)pChannel, pOutInfo);
    }
    else if (instanceof(pChannel, DirectoryChannel)) {
        err = DirectoryChannel_GetInfo((DirectoryChannelRef)pChannel, pOutInfo);
    }
    else {
        err = EBADF;
    }

    return err;
}

// Modifies information about the file at the given path.
errno_t FileManager_SetFileMode(FileManagerRef _Nonnull self, const char* _Nonnull path, mode_t mode)
{
    decl_try_err();
    ResolvedPath r;

    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r)) == EOK) {
        // Only the owner of a file may change its metadata.
        
        Inode_Lock(r.inode);
        err = SecurityManager_CheckNodeStatusUpdatePermission(gSecurityManager, r.inode, self->ruid);
        if (err == EOK) {
            err = Inode_SetMode(r.inode, mode);
        }
        Inode_Unlock(r.inode);
    }

    ResolvedPath_Deinit(&r);
    
    return err;
}

// Modifies information about the file at the given path.
errno_t FileManager_SetFileOwner(FileManagerRef _Nonnull self, const char* _Nonnull path, uid_t uid, gid_t gid)
{
    decl_try_err();
    ResolvedPath r;

    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r)) == EOK) {
        // Only the owner of a file may change its metadata.
        
        Inode_Lock(r.inode);
        err = SecurityManager_CheckNodeStatusUpdatePermission(gSecurityManager, r.inode, self->ruid);
        if (err == EOK) {
            err = Inode_SetOwner(r.inode, uid, gid);
        }
        Inode_Unlock(r.inode);
    }

    ResolvedPath_Deinit(&r);
    
    return err;
}

errno_t FileManager_SetFileTimestamps(FileManagerRef _Nonnull self, const char* _Nonnull path, const struct timespec times[_Nullable 2])
{
    decl_try_err();
    ResolvedPath r;

    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r)) == EOK) {
        // Only the owner of a file may change its metadata.
        const int isTotalOmit = (times && times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT);
        
        if (!isTotalOmit) {
           Inode_Lock(r.inode);
            err = SecurityManager_CheckNodeStatusUpdatePermission(gSecurityManager, r.inode, self->ruid);
            if (err == EOK) {
                err = Inode_SetTimes(r.inode, times);
            }
            Inode_Unlock(r.inode);
        }
    }

    ResolvedPath_Deinit(&r);
    
    return err;
}

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
errno_t FileManager_TruncateFile(FileManagerRef _Nonnull self, const char* _Nonnull path, off_t length)
{
    decl_try_err();
    ResolvedPath r;

    if (length < 0ll) {
        return EINVAL;
    }
    
    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r)) == EOK) {
        Inode_Lock(r.inode);
        if (Inode_IsRegularFile(r.inode)) {
            err = SecurityManager_CheckNodeAccess(gSecurityManager, r.inode, self->ruid, self->rgid, W_OK);
            if (err == EOK) {
                err = Inode_Truncate(r.inode, length);
            }
        }
        else {
            err = EISDIR;
        }
        Inode_Unlock(r.inode);
    }

    ResolvedPath_Deinit(&r);

    return err;
}

// Same as above but the file is identified by the given I/O channel.
errno_t FileManager_TruncateFile_ioc(FileManagerRef _Nonnull self, IOChannelRef _Nonnull pChannel, off_t length)
{
    decl_try_err();

    if (instanceof(pChannel, FileChannel)) {
        err = FileChannel_Truncate((FileChannelRef)pChannel, length);
    }
    else if (instanceof(pChannel, DirectoryChannel)) {
        err = EISDIR;
    }
    else {
        err = ENOTDIR;
    }

    return err;
}

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
errno_t FileManager_CheckAccess(FileManagerRef _Nonnull self, const char* _Nonnull path, int mode)
{
    decl_try_err();
    ResolvedPath r;

    if ((err = FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_Target, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r)) == EOK) {
        if (mode != F_OK) {
            Inode_Lock(r.inode);
            err = SecurityManager_CheckNodeAccess(gSecurityManager, r.inode, self->ruid, self->rgid, mode);
            Inode_Unlock(r.inode);
        }
    }

    ResolvedPath_Deinit(&r);

    return err;
}

// Unlinks the inode at the path 'path'.
errno_t FileManager_Unlink(FileManagerRef _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    ResolvedPath r;
    InodeRef dir = NULL;
    InodeRef target = NULL;

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, path, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &r));

    const PathComponent* name = &r.lastPathComponent;
    dir = r.inode;
    r.inode = NULL;
    Inode_Lock(dir);


    // A path that ends in . or .. is not legal
    if ((name->count == 1 && name->name[0] == '.')
        || (name->count == 2 && name->name[0] == '.' && name->name[1] == '.')) {
        throw(EINVAL);
    }


    // Figure out what the target and parent node is
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(dir), dir, name, NULL, &target));
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


    // We must have write permissions for 'pDir'
    try(SecurityManager_CheckNodeAccess(gSecurityManager, dir, self->ruid, self->rgid, W_OK));


    try(Filesystem_Unlink(Inode_GetFilesystem(target), target, dir));

catch:
    Inode_UnlockRelinquish(target);
    Inode_UnlockRelinquish(dir);

    ResolvedPath_Deinit(&r);

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
errno_t FileManager_Rename(FileManagerRef _Nonnull self, const char* oldPath, const char* newPath)
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

    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, oldPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &or));
    try(FileHierarchy_AcquireNodeForPath(self->fileHierarchy, kPathResolution_PredecessorOfTarget, newPath, self->rootDirectory, self->workingDirectory, self->ruid, self->rgid, &nr));

    const PathComponent* oldName = &or.lastPathComponent;
    const PathComponent* newName = &nr.lastPathComponent;


    // Final path components of . and .. are not supported
    if ((oldName->count == 1 && oldName->name[0] == '.')
        || (oldName->count == 2 && oldName->name[0] == '.' && oldName->name[1] == '.')) {
        throw(EINVAL);
    }
    if ((newName->count == 1 && newName->name[0] == '.')
        || (newName->count == 2 && newName->name[0] == '.' && newName->name[1] == '.')) {
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
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(oldDir), oldDir, oldName, NULL, &oldNode));
    ilock_ordered(oldNode, lockedNodes, &lockedNodeCount);


    // The destination may exist
    err = Filesystem_AcquireNodeForName(Inode_GetFilesystem(newDir), newDir, newName, &dih, &newNode);
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
    try(SecurityManager_CheckNodeAccess(gSecurityManager, oldDir, self->ruid, self->rgid, W_OK));
    if (oldDir != newDir) {
        try(SecurityManager_CheckNodeAccess(gSecurityManager, newDir, self->ruid, self->rgid, W_OK));
    }


    // Remove the destination node if it exists
    if (newNode) {
        try(Filesystem_Unlink(Inode_GetFilesystem(newNode), newNode, newDir));
    }


    // Do the move or rename
    if (isMove) {
        err = Filesystem_Move(Inode_GetFilesystem(oldNode), oldNode, oldDir, newDir, newName, self->ruid, self->rgid, &dih);
    }
    else {
        err = Filesystem_Rename(Inode_GetFilesystem(oldNode), oldNode, oldDir, newName, self->ruid, self->rgid);
    }

catch:
    iunlock_ordered_all(lockedNodes, &lockedNodeCount);

    Inode_Relinquish(newNode);
    Inode_Relinquish(newDir);
    Inode_Relinquish(oldNode);
    Inode_Relinquish(oldDir);
    
    ResolvedPath_Deinit(&nr);
    ResolvedPath_Deinit(&or);
    
    return err;
}
