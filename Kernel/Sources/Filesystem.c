//
//  Filesystem.c
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Filesystem.h"


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Path Component
////////////////////////////////////////////////////////////////////////////////

// Initializes a path component from a NUL-terminated string
PathComponent PathComponent_MakeFromCString(const Character* _Nonnull pCString)
{
    PathComponent pc;

    pc.name = pCString;
    pc.count = String_Length(pCString);
    return pc;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: File
////////////////////////////////////////////////////////////////////////////////

ErrorCode File_Create(FilesystemRef _Nonnull pFilesystem, UInt mode, InodeRef _Nonnull pNode, FileRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileRef pFile;

    try(IOChannel_AbstractCreate(&kFileClass, (IOResourceRef)pFilesystem, mode, (IOChannelRef*)&pFile));
    pFile->inode = Object_RetainAs(pNode, Inode);
    pFile->offset = 0ll;

catch:
    *pOutFile = pFile;
    return err;
}

// Creates a copy of the given file.
ErrorCode File_CreateCopy(FileRef _Nonnull pInFile, FileRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FileRef pNewFile;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)pInFile, (IOChannelRef*)&pNewFile));
    pNewFile->inode = Object_RetainAs(pInFile->inode, Inode);
    pNewFile->offset = pInFile->offset;

catch:
    *pOutFile = pNewFile;
    return err;
}

void File_deinit(FileRef _Nonnull self)
{
    Object_Release(self->inode);
    self->inode = NULL;
}

ErrorCode File_seek(FileRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutPosition, Int whence)
{
    if(pOutPosition) {
        *pOutPosition = self->offset;
    }

    FileOffset newOffset;

    switch (whence) {
        case SEEK_SET:
            newOffset = offset;
            break;

        case SEEK_CUR:
            newOffset = self->offset + offset;
            break;

        case SEEK_END:
            newOffset = Inode_GetFileSize(self->inode) + offset;
            break;

        default:
            return EINVAL;
    }

    if (newOffset < 0) {
        return EINVAL;
    }
    // XXX do overflow check

    self->offset = newOffset;
    return EOK;
}

CLASS_METHODS(File, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, File, Object)
OVERRIDE_METHOD_IMPL(seek, File, IOChannel)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Directory
////////////////////////////////////////////////////////////////////////////////

ErrorCode Directory_Create(FilesystemRef _Nonnull pFilesystem, InodeRef _Nonnull pNode, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryRef pDir;

    try(IOChannel_AbstractCreate(&kDirectoryClass, (IOResourceRef)pFilesystem, FREAD, (IOChannelRef*)&pDir));
    pDir->inode = Object_RetainAs(pNode, Inode);
    pDir->offset = 0ll;

catch:
    *pOutDir = pDir;
    return err;
}

// Creates a copy of the given directory descriptor.
ErrorCode Directory_CreateCopy(DirectoryRef _Nonnull pInDir, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryRef pNewDir;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)pInDir, (IOChannelRef*)&pNewDir));
    pNewDir->inode = Object_RetainAs(pInDir->inode, Inode);
    pNewDir->offset = pInDir->offset;

catch:
    *pOutDir = pNewDir;
    return err;
}

void Directory_deinit(DirectoryRef _Nonnull self)
{
    Object_Release(self->inode);
    self->inode = NULL;
}

ByteCount Directory_dup(DirectoryRef _Nonnull self, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    return EBADF;
}

ByteCount Directory_command(DirectoryRef _Nonnull self, Int op, va_list ap)
{
    return EBADF;
}

ByteCount Directory_read(DirectoryRef _Nonnull self, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    return Filesystem_ReadDirectory(IOChannel_GetResource(self), self, pBuffer, nBytesToRead);
}

ByteCount Directory_write(DirectoryRef _Nonnull self, const Byte* _Nonnull pBuffer, ByteCount nBytesToWrite)
{
    return EBADF;
}

ErrorCode Directory_seek(DirectoryRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutPosition, Int whence)
{
    
    if(pOutPosition) {
        *pOutPosition = self->offset;
    }
    if (whence != SEEK_SET || offset < 0) {
        return EINVAL;
    }
    if (offset > (FileOffset)INT_MAX) {
        return EOVERFLOW;
    }

    self->offset = offset;
    return EOK;
}

ErrorCode Directory_close(DirectoryRef _Nonnull self)
{
    return Filesystem_CloseDirectory(IOChannel_GetResource(self), self);
}

CLASS_METHODS(Directory, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, Directory, Object)
OVERRIDE_METHOD_IMPL(dup, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(command, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(read, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(write, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(seek, Directory, IOChannel)
OVERRIDE_METHOD_IMPL(close, Directory, IOChannel)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

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
ErrorCode Filesystem_Create(ClassRef pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    FilesystemRef pFileSys;

    try(_Object_Create(pClass, 0, (ObjectRef*)&pFileSys));
    pFileSys->fsid = Filesystem_GetNextAvailableId();
    *pOutFileSys = pFileSys;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

// Invoked when an instance of this file system is mounted.
ErrorCode Filesystem_onMount(FilesystemRef _Nonnull self, const Byte* _Nonnull pParams, ByteCount paramsSize)
{
    return EOK;
}

// Invoked when a mounted instance of this file system is unmounted. A file
// system may return an error. Note however that this error is purely advisory
// and the file system implementation is required to do everything it can to
// successfully unmount. Unmount errors are ignored and the file system manager
// will complete the unmount in any case.
ErrorCode Filesystem_onUnmount(FilesystemRef _Nonnull self)
{
    return EOK;
}


// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
InodeRef _Nonnull Filesystem_copyRootNode(FilesystemRef _Nonnull self)
{
    abort();
    // NOT REACHED
    return NULL;
}

// Returns EOK and the parent node of the given node if it exists and ENOENT
// and NULL if the given node is the root node of the namespace. 
ErrorCode Filesystem_copyParentOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function will always only be called with proper node names. Eg never with
// "." nor "..".
ErrorCode Filesystem_copyNodeForName(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pComponent, User user, InodeRef _Nullable * _Nonnull pOutNode)
{
    *pOutNode = NULL;
    return ENOENT;
}

ErrorCode Filesystem_getNameOfNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, InodeRef _Nonnull pNode, User user, MutablePathComponent* _Nonnull pComponent)
{
    pComponent->count = 0;
    return ENOENT;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
ErrorCode Filesystem_getFileInfo(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, FileInfo* _Nonnull pOutInfo)
{
    return ENOENT;
}

// If the node is a directory and another file system is mounted at this directory,
// then this function returns the filesystem ID of the mounted directory; otherwise
// 0 is returned.
FilesystemId Filesystem_getFilesystemMountedOnNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode)
{
    return 0;
}

// Marks the given directory node as a mount point at which the filesystem
// with the given filesystem ID is mounted. Converts the node back into a
// regular directory node if the give filesystem ID is 0.
void Filesystem_setFilesystemMountedOnNode(FilesystemRef _Nonnull self, InodeRef _Nonnull pNode, FilesystemId fsid)
{
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions
ErrorCode Filesystem_createDirectory(FilesystemRef _Nonnull self, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pName, User user, FilePermissions permissions)
{
    return EACCESS;
}

// Opens the directory represented by the given node. Returns a directory
// descriptor object which is teh I/O channel that allows you to read the
// directory content.
ErrorCode Filesystem_openDirectory(FilesystemRef _Nonnull self, InodeRef _Nonnull pDirNode, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    return Directory_Create(self, pDirNode, pOutDir);
}

// Reads the next set of directory entries. The first entry read is the one
// at the current directory index stored in 'pDir'. This function guarantees
// that it will only ever return complete directories entries. It will never
// return a partial entry. Consequently the provided buffer must be big enough
// to hold at least one directory entry.
ByteCount Filesystem_readDirectory(FilesystemRef _Nonnull self, DirectoryRef _Nonnull pDir, Byte* _Nonnull pBuffer, ByteCount nBytesToRead)
{
    return -EIO;
}

// Closes the given directory I/O channel.
ErrorCode Filesystem_closeDirectory(FilesystemRef _Nonnull self, DirectoryRef _Nonnull pDir)
{
    Object_Release(pDir);
    return EOK;
}


CLASS_METHODS(Filesystem, IOResource,
METHOD_IMPL(onMount, Filesystem)
METHOD_IMPL(onUnmount, Filesystem)
METHOD_IMPL(copyRootNode, Filesystem)
METHOD_IMPL(copyParentOfNode, Filesystem)
METHOD_IMPL(copyNodeForName, Filesystem)
METHOD_IMPL(getNameOfNode, Filesystem)
METHOD_IMPL(getFileInfo, Filesystem)
METHOD_IMPL(getFilesystemMountedOnNode, Filesystem)
METHOD_IMPL(setFilesystemMountedOnNode, Filesystem)
METHOD_IMPL(createDirectory, Filesystem)
METHOD_IMPL(openDirectory, Filesystem)
METHOD_IMPL(readDirectory, Filesystem)
METHOD_IMPL(closeDirectory, Filesystem)
);
