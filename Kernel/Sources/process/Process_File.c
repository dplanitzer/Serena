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
    InodeRef pInode = NULL;
    IOChannelRef pFile = NULL;

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOrParent, pPath, &r));
    if (r.target != NULL) {
        // File exists - reject the operation in exclusive mode and open the
        // file otherwise
        if ((options & kOpen_Exclusive) == kOpen_Exclusive) {
            // Exclusive mode: File already exists -> throw an error
            throw(EEXIST);
        }

        try(Filesystem_OpenFile(Inode_GetFilesystem(r.target), r.target, options, pr.user));
        pInode = r.target;
        r.target = NULL;
    }
    else {
        // File does not exist - create it
        FilesystemRef pFS = Inode_GetFilesystem(r.parent);
        FilePermissions filePerms = ~pProc->fileCreationMask & (permissions & 0777);


        // The user provided read/write mode must match up with the provided (user) permissions
        if ((options & kOpen_ReadWrite) == 0) {
            throw(EACCESS);
        }
        if ((options & kOpen_Read) == kOpen_Read && !FilePermissions_Has(filePerms, kFilePermissionsClass_User, kFilePermission_Read)) {
            throw(EACCESS);
        }
        if ((options & kOpen_Write) == kOpen_Write && !FilePermissions_Has(filePerms, kFilePermissionsClass_User, kFilePermission_Write)) {
            throw(EACCESS);
        }


        // Create the new file and add it to its parent directory
        try(Filesystem_CreateNode(pFS, kFileType_RegularFile, pr.user, filePerms, r.parent, &r.lastPathComponent, &r.insertionHint, &pInode));
    }


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
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOnly, pPath, &r));
    try(Filesystem_OpenFile(Inode_GetFilesystem(r.target), r.target, options, pr.user));
    // Note that this call takes ownership of the inode reference
    try(FileChannel_Create(r.target, options, &pFile));
    r.target = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
    pFile = NULL;
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    IOChannel_Release(pFile);
    *pOutIoc = -1;
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
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOnly, pPath, &r)) == EOK) {
        err = Filesystem_GetFileInfo(Inode_GetFilesystem(r.target), r.target, pOutInfo);
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
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOnly, pPath, &r)) == EOK) {
        err = Filesystem_SetFileInfo(Inode_GetFilesystem(r.target), r.target, pr.user, pInfo);
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
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOnly, pPath, &r)) == EOK) {
        err = Filesystem_Truncate(Inode_GetFilesystem(r.target), r.target, pr.user, length);
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
    if ((err = PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetOnly, pPath, &r)) == EOK) {
        if (mode != kAccess_Exists) {
            err = Filesystem_CheckAccess(Inode_GetFilesystem(r.target), r.target, pr.user, mode);
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

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetAndParent, pPath, &r));

    if (Inode_IsDirectory(r.target)) {
        // Can not unlink a mountpoint
        if (FilesystemManager_IsNodeMountpoint(gFilesystemManager, r.target)) {
            throw(EBUSY);
        }


        // Can not unlink the root of a filesystem
        if (Inode_GetId(r.target) == Inode_GetId(r.parent)) {
            throw(EBUSY);
        }


        // Can not unlink the process' root directory
        if (Inode_Equals(pProc->rootDirectory, r.target)) {
            throw(EBUSY);
        }
    }

    try(Filesystem_Unlink(Inode_GetFilesystem(r.target), r.target, r.parent, pr.user));

catch:
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

    Lock_Lock(&pProc->lock);
    Process_MakePathResolver(pProc, &pr);
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_TargetAndParent, pOldPath, &sr));
    try(PathResolver_AcquireNodeForPath(&pr, kPathResolverMode_OptionalTargetAndParent, pNewPath, &tr));

    // newpath and oldpath have to be in the same filesystem
    if (Inode_GetFilesystem(sr.parent) != Inode_GetFilesystem(tr.parent)) {
        throw(EXDEV);
    }

    // Can not rename a mount point
    // XXX implement this check once we've refined the mount point handling (return EBUSY)

    // newpath can't be a child of oldpath
    // XXX implement me

    // unlink the target node if one exists for newpath
    // XXX implement me
    
    try(Filesystem_Rename(Inode_GetFilesystem(sr.parent), sr.target, sr.parent, &tr.lastPathComponent, tr.parent, pr.user));

catch:
    PathResolverResult_Deinit(&tr);
    PathResolverResult_Deinit(&sr);
    PathResolver_Deinit(&pr);
    Lock_Unlock(&pProc->lock);
    err = ENOSYS;   // XXX for now
    return err;
}
