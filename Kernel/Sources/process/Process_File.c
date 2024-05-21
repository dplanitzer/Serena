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
#include <driver/DriverManager.h>
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
errno_t Process_CreateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, FilePermissions permissions, int* _Nonnull pOutIoc)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    DirectoryEntryInsertionHint dih;
    InodeRef pInode = NULL;
    IOChannelRef pFile = NULL;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_PredecessorOfTarget, pPath, &r));

    FilesystemRef pFS = Inode_GetFilesystem(r.inode);
    InodeRef pParentDir = r.inode;
    const PathComponent* pName = &r.lastPathComponent;


    // Can not create a file with names . or ..
    if ((pName->count == 1 && pName->name[0] == '.')
        || (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.')) {
        throw(EISDIR);
    }

    Inode_Lock(pParentDir);


    // The last path component must not exist
    err = Filesystem_AcquireNodeForName(pFS, pParentDir, pName, pr.user, &dih, &pInode);
    if (err == EOK) {
        // File exists - reject the operation in exclusive mode and open the
        // file otherwise
        if ((options & kOpen_Exclusive) == kOpen_Exclusive) {
            // Exclusive mode: File already exists -> throw an error
            err = EEXIST;
        }
        else {
            Inode_Lock(pInode);
            err = Filesystem_OpenFile(pFS, pInode, options, pr.user);
            Inode_Unlock(pInode);
        }
    }
    else {
        // File does not exist - create it
        const FilePermissions filePerms = ~pProc->fileCreationMask & (permissions & 0777);


        // The user provided read/write mode must match up with the provided (user) permissions
        if ((options & kOpen_ReadWrite) == 0) {
            err = EACCESS;
        }
        if (err == EOK && (options & kOpen_Read) == kOpen_Read && !FilePermissions_Has(filePerms, kFilePermissionsClass_User, kFilePermission_Read)) {
            err = EACCESS;
        }
        if (err == EOK && (options & kOpen_Write) == kOpen_Write && !FilePermissions_Has(filePerms, kFilePermissionsClass_User, kFilePermission_Write)) {
            err = EACCESS;
        }


        // Create the new file and add it to its parent directory
        if (err == EOK) {
            err = Filesystem_CreateNode(pFS, kFileType_RegularFile, pr.user, filePerms, pParentDir, pName, &dih, &pInode);
        }
    }

    Inode_Unlock(pParentDir);
    throw_iferr(err);


    // Note that the file channel takes ownership of the inode reference
    try(FileChannel_Create(pInode, options, &pFile));
    pInode = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
    pFile = NULL;

    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
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
errno_t Process_OpenFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, int* _Nonnull pOutIoc)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult r;
    IOChannelRef pFile = NULL;

    Lock_Lock(&pProc->lock);

    // XXX tmp
    if (String_Equals(pPath, "/dev/console")) {
        ConsoleRef pConsole;

        try_null(pConsole, (ConsoleRef) DriverManager_GetDriverForName(gDriverManager, kConsoleName), ENODEV);
        try(ConsoleChannel_Create((ObjectRef)pConsole, options, &pFile));
        try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
        pFile = NULL;
        Lock_Unlock(&pProc->lock);
        return EOK;
    }
    // XXX tmp

    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pPath, &r));

    Inode_Lock(r.inode);
    err = Filesystem_OpenFile(Inode_GetFilesystem(r.inode), r.inode, options, pr.user);
    Inode_Unlock(r.inode);
    throw_iferr(err);

    // Note that this call takes ownership of the inode reference
    try(FileChannel_Create(r.inode, options, &pFile));
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

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_Target, pPath, &r)) == EOK) {
        Inode_Lock(r.inode);
        err = Filesystem_Truncate(Inode_GetFilesystem(r.inode), r.inode, pr.user, length);
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
errno_t Process_CheckFileAccess(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, AccessMode mode)
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


    // A path that ends in . or .. is not legal
    if ((pName->count == 1 && pName->name[0] == '.')
        || (pName->count == 2 && pName->name[0] == '.' && pName->name[1] == '.')) {
        throw(EINVAL);
    }

    pDir = r.inode;
    r.inode = NULL;
    Inode_Lock(pDir);

    // Figure out what the target and parent node is
    err = Filesystem_AcquireNodeForName(Inode_GetFilesystem(pDir), pDir, pName, pr.user, NULL, &pTarget);
    if (err == EOK) {
        Inode_Lock(pTarget);

        if (Inode_IsDirectory(pTarget)) {
            // Can not unlink a mountpoint
            if (FilesystemManager_IsNodeMountpoint(gFilesystemManager, pTarget)) {
                err = EBUSY;
            }


            // Can not unlink the root of a filesystem
            if (err == EOK && (Inode_GetId(pTarget) == Inode_GetId(pDir))) {
                err = EBUSY;
            }


            // Can not unlink the process' root directory
            if (err == EOK && Inode_Equals(pProc->rootDirectory, pTarget)) {
                err = EBUSY;
            }
        }

        if (err == EOK) {
            err = Filesystem_Unlink(Inode_GetFilesystem(pTarget), pTarget, pDir, pr.user);
        }

        Inode_Unlock(pTarget);
    }

    Inode_Unlock(pDir);

catch:
    Inode_Relinquish(pDir);
    Inode_Relinquish(pTarget);
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
errno_t Process_Rename(ProcessRef _Nonnull pProc, const char* pOldPath, const char* pNewPath)
{
    decl_try_err();
    PathResolver pr;
    PathResolverResult sr, tr;
    DirectoryEntryInsertionHint dih;
    InodeRef pOldDir = NULL;
    InodeRef pOldNode = NULL;
    InodeRef pNewDir = NULL;
    InodeRef pNewNode = NULL;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_PredecessorOfTarget, pOldPath, &sr));
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_PredecessorOfTarget, pNewPath, &tr));

    const PathComponent* pOldName = &sr.lastPathComponent;
    const PathComponent* pNewName = &tr.lastPathComponent;


    // Final path components of . and .. are not supported
    if ((pOldName->count == 1 && pOldName->name[0] == '.')
        || (pOldName->count == 2 && pOldName->name[0] == '.' && pOldName->name[1] == '.')) {
        throw(EINVAL);
    }
    if ((pNewName->count == 1 && pNewName->name[0] == '.')
        || (pNewName->count == 2 && pNewName->name[0] == '.' && pNewName->name[1] == '.')) {
        throw(EINVAL);
    }


    // newpath and oldpath have to be in the same filesystem
    if (Inode_GetFilesystem(sr.inode) != Inode_GetFilesystem(tr.inode)) {
        throw(EXDEV);
    }


    // The source file/directory must exist
    pOldDir = sr.inode;
    sr.inode = NULL;
    try(Filesystem_AcquireNodeForName(Inode_GetFilesystem(pOldDir), pOldDir, pOldName, pr.user, NULL, &pOldNode));


    // The target file/directory may exist
    pNewDir = tr.inode;
    tr.inode = NULL;
    err = Filesystem_AcquireNodeForName(Inode_GetFilesystem(pNewDir), pNewDir, pNewName, pr.user, &dih, &pNewNode);
    if (err == EOK) {
        // Unlink the target node
        try(Filesystem_Unlink(Inode_GetFilesystem(pNewNode), pNewNode, tr.inode, pr.user));
    }


    // Can not rename a mount point
    // XXX implement this check once we've refined the mount point handling (return EBUSY)

    // newpath can't be a child of oldpath
    // XXX implement me

    try(Filesystem_Rename(Inode_GetFilesystem(pOldNode), pOldNode, pOldDir, pNewName, pNewDir, pr.user));

catch:
    Inode_Relinquish(pNewNode);
    Inode_Relinquish(pNewDir);
    Inode_Relinquish(pOldNode);
    Inode_Relinquish(pOldDir);
    PathResolverResult_Deinit(&tr);
    PathResolverResult_Deinit(&sr);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    err = ENOSYS;   // XXX for now
    return err;
}
