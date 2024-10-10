//
//  Process_Filesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/FilesystemManager.h>
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FileChannel.h>
// XXX tmp
#include <console/Console.h>
#include <console/ConsoleChannel.h>
#include <driver/DriverCatalog.h>
// XXX tmp


// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
FilePermissions Process_GetFileCreationMask(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const FilePermissions mask = pProc->fileCreationMask;
    Lock_Unlock(&pProc->lock);
    return mask;
}

// Sets the file creation mask of the receiver.
void Process_SetFileCreationMask(ProcessRef _Nonnull pProc, FilePermissions mask)
{
    Lock_Lock(&pProc->lock);
    pProc->fileCreationMask = mask & 0777;
    Lock_Unlock(&pProc->lock);
}

// Creates a file in the given filesystem location.
errno_t Process_CreateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int mode, FilePermissions permissions, int* _Nonnull pOutIoc)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    DirectoryEntryInsertionHint dih;
    InodeRef pDir = NULL;
    InodeRef pInode = NULL;
    IOChannelRef pFile = NULL;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_PredecessorOfTarget, pPath, &r));

    const PathComponent* pName = &r.lastPathComponent;
    FilesystemRef pFS = Inode_GetFilesystem(r.inode);
    pDir = r.inode;
    r.inode = NULL;
    Inode_Lock(pDir);


    // Can not create a file with names . or ..
    if ((pName->count == 1 && pName->name[0] == '.')
        || (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.')) {
        throw(EISDIR);
    }


    // The last path component must not exist
    err = Filesystem_AcquireNodeForName(pFS, pDir, pName, pr.user, &dih, &pInode);
    if (err == EOK) {
        // File exists - reject the operation in exclusive mode and open the
        // file otherwise
        if ((mode & kOpen_Exclusive) == kOpen_Exclusive) {
            // Exclusive mode: File already exists -> throw an error
            throw(EEXIST);
        }
        else {
            Inode_Lock(pInode);
            err = Filesystem_OpenFile(pFS, pInode, mode, pr.user);
            Inode_Unlock(pInode);
            throw_iferr(err);
        }
    }
    else if (err == ENOENT) {
        // File does not exist - create it
        const FilePermissions filePerms = ~pProc->fileCreationMask & (permissions & 0777);


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
        try(Filesystem_CreateNode(pFS, kFileType_RegularFile, pr.user, filePerms, pDir, pName, &dih, &pInode));
    }
    else {
        throw(err);
    }

    Inode_UnlockRelinquish(pDir);
    pDir = NULL;


    // Note that the file channel takes ownership of the inode reference
    try(FileChannel_Create(pInode, mode, &pFile));
    pInode = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
    pFile = NULL;

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    Inode_UnlockRelinquish(pDir);
    IOChannel_Release(pFile);
    Inode_Relinquish(pInode);

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    
    *pOutIoc = -1;
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t Process_OpenFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int mode, int* _Nonnull pOutIoc)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    IOChannelRef pFile = NULL;

    Lock_Lock(&pProc->lock);

    // XXX tmp
    if (String_Equals(pPath, "/dev/console")) {
        ConsoleRef pConsole;

        try_null(pConsole, (ConsoleRef) DriverCatalog_GetDriverForName(gDriverCatalog, kConsoleName), ENODEV);
        try(Driver_Open((DriverRef)pConsole, pPath, mode, &pFile));
        try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
        pFile = NULL;
        Lock_Unlock(&pProc->lock);
        return EOK;
    }
    // XXX tmp

    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pPath, &r));

    Inode_Lock(r.inode);
    err = Filesystem_OpenFile(Inode_GetFilesystem(r.inode), r.inode, mode, pr.user);
    Inode_Unlock(r.inode);
    throw_iferr(err);

    // Note that this call takes ownership of the inode reference
    try(FileChannel_Create(r.inode, mode, &pFile));
    r.inode = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
    pFile = NULL;

catch:
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);

    if (err != EOK) {
        IOChannel_Release(pFile);
        *pOutIoc = -1;
    }

    return err;
}

// Returns information about the file at the given path.
errno_t Process_GetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pPath, &r)) == EOK) {
        Inode_Lock(r.inode);
        err = Filesystem_GetFileInfo(Inode_GetFilesystem(r.inode), r.inode, pOutInfo);
        Inode_Unlock(r.inode);
    }

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_GetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int ioc, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pProc->ioChannelTable, ioc, &pChannel)) == EOK) {
        if (instanceof(pChannel, FileChannel)) {
            err = FileChannel_GetInfo((FileChannelRef)pChannel, pOutInfo);
        }
        else if (instanceof(pChannel, DirectoryChannel)) {
            err = DirectoryChannel_GetInfo((DirectoryChannelRef)pChannel, pOutInfo);
        }
        else {
            err = EBADF;
        }

        IOChannelTable_RelinquishChannel(&pProc->ioChannelTable, pChannel);
    }
    return err;
}

// Modifies information about the file at the given path.
errno_t Process_SetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pPath, &r)) == EOK) {
        Inode_Lock(r.inode);
        err = Filesystem_SetFileInfo(Inode_GetFilesystem(r.inode), r.inode, pr.user, pInfo);
        Inode_Unlock(r.inode);
    }

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_SetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int ioc, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pProc->ioChannelTable, ioc, &pChannel)) == EOK) {
        if (instanceof(pChannel, FileChannel)) {
            err = FileChannel_SetInfo((FileChannelRef)pChannel, pProc->realUser, pInfo);
        }
        else if (instanceof(pChannel, DirectoryChannel)) {
            err = DirectoryChannel_SetInfo((DirectoryChannelRef)pChannel, pProc->realUser, pInfo);
        }
        else {
            err = EBADF;
        }

        IOChannelTable_RelinquishChannel(&pProc->ioChannelTable, pChannel);
    }

    return err;
}

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
errno_t Process_TruncateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileOffset length)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;

    if (length < 0ll) {
        return EINVAL;
    }
    
    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pPath, &r)) == EOK) {
        Inode_Lock(r.inode);
        if (Inode_IsRegularFile(r.inode)) {
            err = Filesystem_TruncateFile(Inode_GetFilesystem(r.inode), r.inode, pr.user, length);
        }
        else {
            err = EISDIR;
        }
        Inode_Unlock(r.inode);
    }

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Same as above but the file is identified by the given I/O channel.
errno_t Process_TruncateFileFromIOChannel(ProcessRef _Nonnull pProc, int ioc, FileOffset length)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = IOChannelTable_AcquireChannel(&pProc->ioChannelTable, ioc, &pChannel)) == EOK) {
        if (instanceof(pChannel, FileChannel)) {
            err = FileChannel_Truncate((FileChannelRef)pChannel, pProc->realUser, length);
        }
        else if (instanceof(pChannel, DirectoryChannel)) {
            err = EISDIR;
        }
        else {
            err = ENOTDIR;
        }

        IOChannelTable_RelinquishChannel(&pProc->ioChannelTable, pChannel);
    }
    return err;
}

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
errno_t Process_CheckAccess(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, AccessMode mode)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pPath, &r)) == EOK) {
        if (mode != kAccess_Exists) {
            Inode_Lock(r.inode);
            err = Filesystem_CheckAccess(Inode_GetFilesystem(r.inode), r.inode, pr.user, mode);
            Inode_Unlock(r.inode);
        }
    }

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Unlinks the inode at the path 'pPath'.
errno_t Process_Unlink(ProcessRef _Nonnull pProc, const char* _Nonnull pPath)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    InodeRef pDir = NULL;
    InodeRef pTarget = NULL;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_PredecessorOfTarget, pPath, &r));

    const PathComponent* pName = &r.lastPathComponent;
    pDir = r.inode;
    r.inode = NULL;
    Inode_Lock(pDir);


    // A path that ends in . or .. is not legal
    if ((pName->count == 1 && pName->name[0] == '.')
        || (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.')) {
        throw(EINVAL);
    }


    // Figure out what the target and parent node is
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pDir), pDir, pName, pr.user, NULL, &pTarget));
    Inode_Lock(pTarget);

    if (Inode_IsDirectory(pTarget)) {
        // Can not unlink a mountpoint
        if (FilesystemManager_IsNodeMountpoint(gFilesystemManager, pTarget)) {
            throw(EBUSY);
        }


        // Can not unlink the root of a filesystem
        if (Inode_GetId(pTarget) == Inode_GetId(pDir)) {
            throw(EBUSY);
        }


        // Can not unlink the process' root directory
        if (Inode_Equals(pProc->rootDirectory, pTarget)) {
            throw(EBUSY);
        }
    }

    try(Filesystem_Unlink(Inode_GetFilesystem(pTarget), pTarget, pDir, pr.user));

catch:
    Inode_UnlockRelinquish(pTarget);
    Inode_UnlockRelinquish(pDir);

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);

    Lock_Unlock(&pProc->lock);
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

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
errno_t Process_Rename(ProcessRef _Nonnull pProc, const char* pOldPath, const char* pNewPath)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult or, nr;
    DirectoryEntryInsertionHint dih;
    InodeRef pOldDir = NULL;
    InodeRef pOldNode = NULL;
    InodeRef pNewDir = NULL;
    InodeRef pNewNode = NULL;
    InodeRef pLockedNodes[4];
    int lockedNodeCount = 0;
    bool isMove = false;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_PredecessorOfTarget, pOldPath, &or));
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_PredecessorOfTarget, pNewPath, &nr));

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
    pOldDir = or.inode;
    or.inode = NULL;
    ilock_ordered(pOldDir, pLockedNodes, &lockedNodeCount);

    pNewDir = nr.inode;
    nr.inode = NULL;
    if (!Inode_Equals(pOldDir, pNewDir)) {
        ilock_ordered(pNewDir, pLockedNodes, &lockedNodeCount);
        isMove = true;
    }


    // newpath and oldpath have to be in the same filesystem
    if (Inode_GetFilesystem(pOldDir) != Inode_GetFilesystem(pNewDir)) {
        throw(EXDEV);
    }


    // Get the source node. It must exist
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pOldDir), pOldDir, pOldName, pr.user, NULL, &pOldNode));
    ilock_ordered(pOldNode, pLockedNodes, &lockedNodeCount);


    // The destination may exist
    err = Filesystem_AcquireNodeForName(Inode_GetFilesystem(pNewDir), pNewDir, pNewName, pr.user, &dih, &pNewNode);
    if (err != EOK && err != ENOENT) {
        throw(err);
    }
    if (err == EOK) {
        if (Inode_Equals(pOldNode, pNewNode)) {
            // Source and destination nodes are the same nodes -> do nothing
            err = EOK;
            goto catch;
        }

        ilock_ordered(pNewNode, pLockedNodes, &lockedNodeCount);
    }


    // Source and destination nodes may not be mountpoints
    if (FilesystemManager_IsNodeMountpoint(gFilesystemManager, pOldNode)) {
        throw(EBUSY);
    }
    if (pNewNode != NULL && FilesystemManager_IsNodeMountpoint(gFilesystemManager, pNewNode)) {
        throw(EBUSY);
    }


    // Make sure that the parent directories are writeable
    try(Filesystem_CheckAccess(Inode_GetFilesystem(pOldDir), pOldDir, pr.user, kAccess_Writable));
    if (pOldDir != pNewDir) {
        try(Filesystem_CheckAccess(Inode_GetFilesystem(pNewDir), pNewDir, pr.user, kAccess_Writable));
    }


    // Remove the destination node if it exists
    if (pNewNode) {
        try(Filesystem_Unlink(Inode_GetFilesystem(pNewNode), pNewNode, pNewDir, pr.user));
    }


    // Do the move or rename
    if (isMove) {
        err = Filesystem_Move(Inode_GetFilesystem(pOldNode), pOldNode, pOldDir, pNewDir, pNewName, pr.user, &dih);
    }
    else {
        err = Filesystem_Rename(Inode_GetFilesystem(pOldNode), pOldNode, pOldDir, pNewName, pr.user);
    }

catch:
    iunlock_ordered_all(pLockedNodes, &lockedNodeCount);

    Inode_Relinquish(pNewNode);
    Inode_Relinquish(pNewDir);
    Inode_Relinquish(pOldNode);
    Inode_Relinquish(pOldDir);
    
    PathResolverResult_Deinit(&nr);
    PathResolverResult_Deinit(&or);
    PathResolver_Deinit(&pr);
    
    Lock_Unlock(&pProc->lock);
    return err;
}
