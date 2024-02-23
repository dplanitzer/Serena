//
//  Process.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "FilesystemManager.h"
#include "Pipe.h"


CLASS_METHODS(Process, Object,
OVERRIDE_METHOD_IMPL(deinit, Process, Object)
);


// Returns the next PID available for use by a new process.
static ProcessId Process_GetNextAvailablePID(void)
{
    static volatile AtomicInt gNextAvailablePid = 0;
    return AtomicInt_Increment(&gNextAvailablePid);
}

// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
ProcessRef _Nullable Process_GetCurrent(void)
{
    DispatchQueueRef pQueue = DispatchQueue_GetCurrent();

    return (pQueue != NULL) ? DispatchQueue_GetOwningProcess(pQueue) : NULL;
}


errno_t RootProcess_Create(ProcessRef _Nullable * _Nonnull pOutProc)
{
    User user = {kRootUserId, kRootGroupId};
    FilesystemRef pRootFileSys = FilesystemManager_CopyRootFilesystem(gFilesystemManager);
    InodeRef pRootDir = NULL;
    decl_try_err();

    try(Filesystem_AcquireRootNode(pRootFileSys, &pRootDir));
    try(Process_Create(1, user, pRootDir, pRootDir, FilePermissions_MakeFromOctal(0022), pOutProc));
    Filesystem_RelinquishNode(pRootFileSys, pRootDir);

    return EOK;

catch:
    Object_Release(pRootFileSys);
    *pOutProc = NULL;
    return err;
}

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param pProc the process into which the executable image should be loaded
// \param pExecAddr pointer to a GemDOS formatted executable file in memory
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be located at the address 'pExecAddr'
errno_t RootProcess_Exec(ProcessRef _Nonnull pProc, void* _Nonnull pExecAddr)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = Process_Exec_Locked(pProc, pExecAddr, NULL, NULL);
    Lock_Unlock(&pProc->lock);
    return err;
}



errno_t Process_Create(int ppid, User user, InodeRef _Nonnull  pRootDir, InodeRef _Nonnull pCurDir, FilePermissions fileCreationMask, ProcessRef _Nullable * _Nonnull pOutProc)
{
    decl_try_err();
    ProcessRef pProc;
    DispatchQueueRef pMainDispatchQueue = NULL;
    int mainDispatchQueueDesc = -1;
    
    try(Object_Create(Process, &pProc));

    Lock_Init(&pProc->lock);

    pProc->ppid = ppid;
    pProc->pid = Process_GetNextAvailablePID();

    try(ObjectArray_Init(&pProc->ioChannels, INITIAL_IOCHANNELS_CAPACITY));
    try(ObjectArray_Init(&pProc->privateResources, INITIAL_PRIVATE_RESOURCES_CAPACITY));
    try(IntArray_Init(&pProc->childPids, 0));

    try(PathResolver_Init(&pProc->pathResolver, pRootDir, pCurDir));
    pProc->fileCreationMask = fileCreationMask;
    pProc->realUser = user;

    List_Init(&pProc->tombstones);
    ConditionVariable_Init(&pProc->tombstoneSignaler);

    try(DispatchQueue_Create(0, 1, DISPATCH_QOS_INTERACTIVE, DISPATCH_PRIORITY_NORMAL, gVirtualProcessorPool, pProc, &pMainDispatchQueue));
    try(Process_RegisterPrivateResource_Locked(pProc, (ObjectRef) pMainDispatchQueue, &mainDispatchQueueDesc));
    Object_Release(pMainDispatchQueue);
    pProc->mainDispatchQueue = pMainDispatchQueue;
    pMainDispatchQueue = NULL;
    assert(mainDispatchQueueDesc == 0);

    try(AddressSpace_Create(&pProc->addressSpace));

    *pOutProc = pProc;
    return EOK;

catch:
    Object_Release(pMainDispatchQueue);
    Object_Release(pProc);
    *pOutProc = NULL;
    return err;
}

void Process_deinit(ProcessRef _Nonnull pProc)
{
    Process_CloseAllIOChannels_Locked(pProc);
    ObjectArray_Deinit(&pProc->ioChannels);

    Process_DisposeAllPrivateResources_Locked(pProc);
    ObjectArray_Deinit(&pProc->privateResources);

    PathResolver_Deinit(&pProc->pathResolver);

    Process_DestroyAllTombstones_Locked(pProc);
    ConditionVariable_Deinit(&pProc->tombstoneSignaler);
    IntArray_Deinit(&pProc->childPids);

    AddressSpace_Destroy(pProc->addressSpace);
    pProc->addressSpace = NULL;
    pProc->imageBase = NULL;
    pProc->argumentsBase = NULL;
    pProc->mainDispatchQueue = NULL;

    pProc->pid = 0;
    pProc->ppid = 0;

    Lock_Deinit(&pProc->lock);
}

ProcessId Process_GetId(ProcessRef _Nonnull pProc)
{
    // The PID is constant over the lifetime of the process. No need to lock here
    return pProc->pid;
}

ProcessId Process_GetParentId(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const ProcessId ppid = pProc->ppid;
    Lock_Unlock(&pProc->lock);

    return ppid;
}

UserId Process_GetRealUserId(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    const UserId uid = pProc->realUser.uid;
    Lock_Unlock(&pProc->lock);

    return uid;
}

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
void* _Nonnull Process_GetArgumentsBaseAddress(ProcessRef _Nonnull pProc)
{
    Lock_Lock(&pProc->lock);
    void* ptr = pProc->argumentsBase;
    Lock_Unlock(&pProc->lock);
    return ptr;
}

errno_t Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, int od, Closure1Arg_Func _Nonnull pUserClosure, void* _Nullable pContext)
{
    decl_try_err();
    DispatchQueueRef pQueue;

    // XXX optimize this: we can actually hold the process lock while dispatching the closure
    // because it will be executed asynchronously anyway. Thus we won't have to retain and
    // release the queue (which is safe to do if we hold the lock until we're done with dispatching)
    if ((err = Process_CopyPrivateResourceForDescriptor(pProc, od, (ObjectRef*) &pQueue)) == EOK) {
        if (Object_InstanceOf(pQueue, DispatchQueue)) {
            err = DispatchQueue_DispatchAsync(pQueue, DispatchQueueClosure_MakeUser(pUserClosure, pContext));
        } else {
            err = EBADF;
        }
        Object_Release(pQueue);
    }
    return err;
}

// Allocates more (user) address space to the given process.
errno_t Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ssize_t count, void* _Nullable * _Nonnull pOutMem)
{
    return AddressSpace_Allocate(pProc->addressSpace, count, pOutMem);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: File I/O
////////////////////////////////////////////////////////////////////////////////

// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
errno_t Process_SetRootDirectoryPath(ProcessRef _Nonnull pProc, const char* pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_SetRootDirectoryPath(&pProc->pathResolver, pProc->realUser, pPath);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Sets the receiver's current working directory to the given path.
errno_t Process_SetWorkingDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_SetCurrentWorkingDirectoryPath(&pProc->pathResolver, pProc->realUser, pPath);
    Lock_Unlock(&pProc->lock);

    return err;
}

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
errno_t Process_GetWorkingDirectory(ProcessRef _Nonnull pProc, char* _Nonnull pBuffer, size_t bufferSize)
{
    Lock_Lock(&pProc->lock);
    const errno_t err = PathResolver_GetCurrentWorkingDirectoryPath(&pProc->pathResolver, pProc->realUser, pBuffer, bufferSize);
    Lock_Unlock(&pProc->lock);

    return err;
}

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
errno_t Process_CreateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, FilePermissions permissions, int* _Nonnull pOutDescriptor)
{
    decl_try_err();
    PathResolverResult r;
    InodeRef _Locked pFileNode = NULL;
    FileRef pFile = NULL;

    Lock_Lock(&pProc->lock);

    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r));
    try(Filesystem_CreateFile(r.filesystem, &r.lastPathComponent, r.inode, pProc->realUser, options, ~pProc->fileCreationMask & (permissions & 0777), &pFileNode));
    try(IOResource_Open(r.filesystem, pFileNode, options, pProc->realUser, (IOChannelRef*)&pFile));
    try(Process_RegisterIOChannel_Locked(pProc, (IOChannelRef)pFile, pOutDescriptor));

catch:
    Object_Release(pFile);
    if (pFileNode) {
        Filesystem_RelinquishNode(r.filesystem, pFileNode);
    }
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return err;
}

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
errno_t Process_Open(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, int* _Nonnull pOutDescriptor)
{
    decl_try_err();
    PathResolverResult r;
    FileRef pFile;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(IOResource_Open(r.filesystem, r.inode, options, pProc->realUser, (IOChannelRef*)&pFile));
    try(Process_RegisterIOChannel_Locked(pProc, (IOChannelRef)pFile, pOutDescriptor));
    Object_Release(pFile);
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    Object_Release(pFile);
    *pOutDescriptor = -1;
    return err;
}

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
errno_t Process_CreateDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FilePermissions permissions)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);

    if ((err = PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_CreateDirectory(r.filesystem, &r.lastPathComponent, r.inode, pProc->realUser, ~pProc->fileCreationMask & (permissions & 0777));
    }
    PathResolverResult_Deinit(&r);

    Lock_Unlock(&pProc->lock);

    return err;
}

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
errno_t Process_OpenDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, int* _Nonnull pOutDescriptor)
{
    decl_try_err();
    PathResolverResult r;
    DirectoryRef pDir = NULL;

    Lock_Lock(&pProc->lock);
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r));
    try(Filesystem_OpenDirectory(r.filesystem, r.inode, pProc->realUser, &pDir));
    try(Process_RegisterIOChannel_Locked(pProc, (IOChannelRef)pDir, pOutDescriptor));
    Object_Release(pDir);
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    PathResolverResult_Deinit(&r);
    Lock_Unlock(&pProc->lock);
    Object_Release(pDir);
    *pOutDescriptor = -1;
    return err;
}

// Creates an anonymous pipe.
errno_t Process_CreatePipe(ProcessRef _Nonnull pProc, int* _Nonnull pOutReadChannel, int* _Nonnull pOutWriteChannel)
{
    decl_try_err();
    PipeRef pPipe = NULL;
    IOChannelRef rdChannel = NULL, wrChannel = NULL;
    bool needsUnlock = false;
    bool isReadChannelRegistered = false;

    try(Pipe_Create(kPipe_DefaultBufferSize, &pPipe));
    try(IOResource_Open(pPipe, NULL /*XXX*/, kOpen_Read, pProc->realUser, &rdChannel));
    try(IOResource_Open(pPipe, NULL /*XXX*/, kOpen_Write, pProc->realUser, &wrChannel));

    Lock_Lock(&pProc->lock);
    needsUnlock = true;
    try(Process_RegisterIOChannel_Locked(pProc, rdChannel, pOutReadChannel));
    isReadChannelRegistered = true;
    try(Process_RegisterIOChannel_Locked(pProc, wrChannel, pOutWriteChannel));
    Object_Release(rdChannel);
    Object_Release(wrChannel);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    if (isReadChannelRegistered) {
        Process_UnregisterIOChannel(pProc, *pOutReadChannel, &rdChannel);
        Object_Release(rdChannel);
    }
    if (needsUnlock) {
        Lock_Unlock(&pProc->lock);
    }
    Object_Release(rdChannel);
    Object_Release(wrChannel);
    Object_Release(pPipe);
    return err;
}

// Returns information about the file at the given path.
errno_t Process_GetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    if ((err = PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_GetFileInfo(r.filesystem, r.inode, pOutInfo);
    }
    PathResolverResult_Deinit(&r);
    
    Lock_Unlock(&pProc->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_GetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int fd, FileInfo* _Nonnull pOutInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel)) == EOK) {
        if (Object_InstanceOf(pChannel, File)) {
            err = Filesystem_GetFileInfo(File_GetFilesystem(pChannel), File_GetInode(pChannel), pOutInfo);
        }
        else if (Object_InstanceOf(pChannel, Directory)) {
            err = Filesystem_GetFileInfo(Directory_GetFilesystem(pChannel), Directory_GetInode(pChannel), pOutInfo);
        }
        else {
            err = EBADF;
        }

        Object_Release(pChannel);
    }
    return err;
}

// Modifies information about the file at the given path.
errno_t Process_SetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    PathResolverResult r;

    Lock_Lock(&pProc->lock);
    if ((err = PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_SetFileInfo(r.filesystem, r.inode, pProc->realUser, pInfo);
    }
    PathResolverResult_Deinit(&r);
    
    Lock_Unlock(&pProc->lock);
    
    return err;
}

// Same as above but with respect to the given I/O channel.
errno_t Process_SetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int fd, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel)) == EOK) {
        if (Object_InstanceOf(pChannel, File)) {
            err = Filesystem_SetFileInfo(File_GetFilesystem(pChannel), File_GetInode(pChannel), pProc->realUser, pInfo);
        }
        else if (Object_InstanceOf(pChannel, Directory)) {
            err = Filesystem_SetFileInfo(Directory_GetFilesystem(pChannel), Directory_GetInode(pChannel), pProc->realUser, pInfo);
        }
        else {
            err = EBADF;
        }

        Object_Release(pChannel);
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
    if ((err = PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
        err = Filesystem_Truncate(r.filesystem, r.inode, pProc->realUser, length);
    }
    PathResolverResult_Deinit(&r);

    Lock_Unlock(&pProc->lock);

    return err;
}

// Same as above but the file is identified by the given I/O channel.
errno_t Process_TruncateFileFromIOChannel(ProcessRef _Nonnull pProc, int fd, FileOffset length)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel)) == EOK) {
        if (Object_InstanceOf(pChannel, File)) {
            err = Filesystem_Truncate(File_GetFilesystem(pChannel), File_GetInode(pChannel), pProc->realUser, length);
        }
        else if (Object_InstanceOf(pChannel, Directory)) {
            err = EISDIR;
        }
        else {
            err = ENOTDIR;
        }

        Object_Release(pChannel);
    }
    return err;
}

// Sends a I/O Channel or I/O Resource defined command to the I/O Channel or
// resource identified by the given descriptor.
errno_t Process_vIOControl(ProcessRef _Nonnull pProc, int fd, int cmd, va_list ap)
{
    decl_try_err();
    IOChannelRef pChannel;

    if ((err = Process_CopyIOChannelForDescriptor(pProc, fd, &pChannel)) == EOK) {
        err = IOChannel_vIOControl(pChannel, cmd, ap);
        Object_Release(pChannel);
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
    if ((err = PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_TargetOnly, pPath, pProc->realUser, &r)) == EOK) {
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
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pPath, pProc->realUser, &r));


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
    if (PathResolver_IsRootDirectory(&pProc->pathResolver, pNodeToUnlink)) {
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
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pOldPath, pProc->realUser, &or));
    try(PathResolver_AcquireNodeForPath(&pProc->pathResolver, kPathResolutionMode_ParentOnly, pNewPath, pProc->realUser, &nr));

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
