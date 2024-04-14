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
    PathResolverResult r;
    InodeRef pInode = NULL;
    IOChannelRef pFile = NULL;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r));
    try(Filesystem_CreateFile(r.filesystem, &r.lastPathComponent, r.inode, pProc->realUser, options, ~pProc->fileCreationMask & (permissions & 0777), &pInode));
    // Note that this call takes ownership of the filesystem and inode references
    try(FileChannel_Create((ObjectRef)r.filesystem, pInode, options, &pFile));
    r.filesystem = NULL;
    pInode = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
    pFile = NULL;
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    IOChannel_Release(pFile);
    Filesystem_RelinquishNode(r.filesystem, pInode);
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    *pOutIoc = -1;
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t Process_OpenFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, int* _Nonnull pOutIoc)
{
    decl_try_err();
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

    try(PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(Filesystem_OpenFile(r.filesystem, r.inode, options, pProc->realUser));
    // Note that this call takes ownership of the filesystem and inode references
    try(FileChannel_Create((ObjectRef)r.filesystem, r.inode, options, &pFile));
    r.filesystem = NULL;
    r.inode = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, pFile, pOutIoc));
    pFile = NULL;
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    IOChannel_Release(pFile);
    *pOutIoc = -1;
    return err;
}

// Returns information about the file at the given path.
errno_t Process_GetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    if ((err = PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_GetFileInfo(r.filesystem, r.inode, pOutInfo);
    }
    PathResolverResult_Deinit(&r);
    
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
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    if ((err = PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_SetFileInfo(r.filesystem, r.inode, pProc->realUser, pInfo);
    }
    PathResolverResult_Deinit(&r);
    
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
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    if ((err = PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_Truncate(r.filesystem, r.inode, pProc->realUser, length);
    }
    PathResolverResult_Deinit(&r);

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
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    if ((err = PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
        if (mode != 0) {
            err = Filesystem_CheckAccess(r.filesystem, r.inode, pProc->realUser, mode);
        }
    }
    PathResolverResult_Deinit(&r);
    
    Lock_Unlock(&pProc->lock);
    return err;
}

// Unlinks the inode at the path 'pPath'.
errno_t Process_Unlink(ProcessRef _Nonnull pProc, const char* _Nonnull pPath)
{
    decl_try_err();
    PathResolverResult r;
    InodeRef _Locked pSecondNode = NULL;
    InodeRef _Weak _Locked pParentNode = NULL;
    InodeRef _Weak _Locked pNodeToUnlink = NULL;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r));


    // Get the inode of the file/directory to unlink. Note that there are two cases here:
    // unlink("."): we need to grab the parent of the directory and make r.inode the node to unlink
    // unlink("anything_else"): r.inode is our parent and we look up the target
    if (r.lastPathComponent.count == 1 && r.lastPathComponent.name[0] == '.') {
        try(Filesystem_AcquireNodeForName(r.filesystem, r.inode, &kPathComponent_Parent, pProc->realUser, &pSecondNode));
        pNodeToUnlink = r.inode;
        pParentNode = pSecondNode;
    }
    else {
        try(Filesystem_AcquireNodeForName(r.filesystem, r.inode, &r.lastPathComponent, pProc->realUser, &pSecondNode));
        pNodeToUnlink = pSecondNode;
        pParentNode = r.inode;
    }


    // Can not unlink a mountpoint
    if (FilesystemManager_IsNodeMountpoint(gFilesystemManager, pNodeToUnlink)) {
        throw(EBUSY);
    }


    // Can not unlink the root of a filesystem
    if (Inode_IsDirectory(pNodeToUnlink) && Inode_GetId(pNodeToUnlink) == Inode_GetId(pParentNode)) {
        throw(EBUSY);
    }


    // Can not unlink the process' root directory
    if (PathResolver_IsRootDirectory(pProc->pathResolver, pNodeToUnlink)) {
        throw(EBUSY);
    }

    try(Filesystem_Unlink(r.filesystem, pNodeToUnlink, pParentNode, pProc->realUser));

catch:
    if (pSecondNode) {
        Filesystem_RelinquishNode(r.filesystem, pSecondNode);
    }
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
errno_t Process_Rename(ProcessRef _Nonnull pProc, const char* pOldPath, const char* pNewPath)
{
    decl_try_err();
    PathResolverResult or, nr;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_ParentOnly, pOldPath, pProc->realUser, &or));
    try(PathResolver_AcquireNodeForPath(pProc->pathResolver, kPathResolutionMode_ParentOnly, pNewPath, pProc->realUser, &nr));

    // Can not rename a mount point
    // XXX implement this check once we've refined the mount point handling (return EBUSY)

    // newpath and oldpath have to be in the same filesystem
    // XXX implement me

    // newpath can't be a child of oldpath
    // XXX implement me

    // unlink the target node if one exists for newpath
    // XXX implement me
    
    try(Filesystem_Rename(or.filesystem, &or.lastPathComponent, or.inode, &nr.lastPathComponent, nr.inode, pProc->realUser));

catch:
    PathResolverResult_Deinit(&or);
    PathResolverResult_Deinit(&nr);
    Lock_Unlock(&pProc->lock);
    return err;
}
