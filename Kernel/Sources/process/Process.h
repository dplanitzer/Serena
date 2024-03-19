//
//  Process.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Process_h
#define Process_h

#include <klib/klib.h>
#include <filesystem/Filesystem.h>
#include <System/Process.h>
#include <User.h>

OPAQUE_CLASS(Process, Object);
typedef struct _ProcessMethodTable {
    ObjectMethodTable   super;
} ProcessMethodTable;


extern ProcessRef _Nonnull  gRootProcess;


// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
extern ProcessRef _Nullable Process_GetCurrent(void);


// Creates the root process which is the first process of the OS.
extern errno_t RootProcess_Create(ProcessRef _Nullable * _Nonnull pOutProc);

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param pProc the process into which the executable image should be loaded
// \param pExecPath path to a GemDOS executable file
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
extern errno_t RootProcess_Exec(ProcessRef _Nonnull pProc, const char* _Nonnull pExecPath);


// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
extern void Process_Terminate(ProcessRef _Nonnull pProc, int exitCode);

// Returns true if the process is marked for termination and false otherwise.
extern bool Process_IsTerminating(ProcessRef _Nonnull pProc);

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if there are no tombstones of terminated
// child processes available or the PID is not the PID of a child process of
// the receiver. Otherwise blocks the caller until the requested process or any
// child process (pid == -1) has exited.
extern errno_t Process_WaitForTerminationOfChild(ProcessRef _Nonnull pProc, ProcessId pid, ProcessTerminationStatus* _Nullable pStatus);

extern int Process_GetId(ProcessRef _Nonnull pProc);
extern int Process_GetParentId(ProcessRef _Nonnull pProc);

extern UserId Process_GetRealUserId(ProcessRef _Nonnull pProc);

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
extern void* _Nonnull Process_GetArgumentsBaseAddress(ProcessRef _Nonnull pProc);

// Spawns a new process that will be a child of the given process. The spawn
// arguments specify how the child process should be created, which arguments
// and environment it will receive and which descriptors it will inherit.
extern errno_t Process_SpawnChildProcess(ProcessRef _Nonnull pProc, const SpawnArguments* _Nonnull pArgs, ProcessId * _Nullable pOutChildPid);


// Dispatches the execution of the given user closure on the given dispatch queue
// with the given options. 
extern errno_t Process_DispatchUserClosure(ProcessRef _Nonnull pProc, int od, unsigned long options, Closure1Arg_Func _Nonnull pUserClosure, void* _Nullable pContext);

// Dispatches the execution of the given user closure on the given dispatch queue
// after the given deadline.
extern errno_t Process_DispatchUserClosureAsyncAfter(ProcessRef _Nonnull pProc, int od, TimeInterval deadline, Closure1Arg_Func _Nonnull pUserClosure, void* _Nullable pContext);

// Returns the dispatch queue associated with the virtual processor on which the
// calling code is running. Note this function assumes that it will ALWAYS be
// called from a system call context and thus the caller will necessarily run in
// the context of a (process owned) dispatch queue.
extern int Process_GetCurrentDispatchQueue(ProcessRef _Nonnull pProc);

// Creates a new dispatch queue and binds it to the process.
extern errno_t Process_CreateDispatchQueue(ProcessRef _Nonnull pProc, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nullable pOutDescriptor);


// Destroys the private resource identified by the given descriptor. The resource
// is deallocated and removed from the resource table.
extern errno_t Process_DisposePrivateResource(ProcessRef _Nonnull pProc, int od);

// Allocates more (user) address space to the given process.
extern errno_t Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ssize_t count, void* _Nullable * _Nonnull pOutMem);


// Registers the given I/O channel with the process. This action allows the
// process to use this I/O channel. The process maintains a strong reference to
// the channel until it is unregistered. Note that the process retains the
// channel and thus you have to release it once the call returns. The call
// returns a descriptor which can be used to refer to the channel from user
// and/or kernel space.
extern errno_t Process_RegisterIOChannel(ProcessRef _Nonnull pProc, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor);

// Unregisters the I/O channel identified by the given descriptor. The channel
// is removed from the process' I/O channel table and a strong reference to the
// channel is returned. The caller should call close() on the channel to close
// it and then release() to release the strong reference to the channel. Closing
// the channel will mark itself as done and the channel will be deallocated once
// the last strong reference to it has been released.
extern errno_t Process_UnregisterIOChannel(ProcessRef _Nonnull pProc, int fd, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Looks up the I/O channel identified by the given descriptor and returns a
// strong reference to it if found. The caller should call release() on the
// channel once it is no longer needed.
extern errno_t Process_CopyIOChannelForDescriptor(ProcessRef _Nonnull pProc, int fd, IOChannelRef _Nullable * _Nonnull pOutChannel);


// Looks up the private resource identified by the given descriptor and returns
// a strong reference to it if found. The caller should call release() on the
// private resource once it is no longer needed.
extern errno_t Process_CopyPrivateResourceForDescriptor(ProcessRef _Nonnull self, int od, ObjectRef _Nullable * _Nonnull pOutResource);


// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
extern errno_t Process_SetRootDirectoryPath(ProcessRef _Nonnull pProc, const char* pPath);

// Sets the receiver's current working directory to the given path.
extern errno_t Process_SetWorkingDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath);

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
extern errno_t Process_GetWorkingDirectory(ProcessRef _Nonnull pProc, char* _Nonnull pBuffer, size_t bufferSize);

// Returns the file creation mask of the receiver. Bits cleared in this mask
// should be removed from the file permissions that user space sent to create a
// file system object (note that this is the compliment of umask).
extern FilePermissions Process_GetFileCreationMask(ProcessRef _Nonnull pProc);

// Sets the file creation mask of the receiver.
extern void Process_SetFileCreationMask(ProcessRef _Nonnull pProc, FilePermissions mask);

// Creates a file in the given filesystem location.
extern errno_t Process_CreateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, FilePermissions permissions, int* _Nonnull pOutDescriptor);

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
extern errno_t Process_Open(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, unsigned int options, int* _Nonnull pOutDescriptor);

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
extern errno_t Process_CreateDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FilePermissions permissions);

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
extern errno_t Process_OpenDirectory(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, int* _Nonnull pOutDescriptor);

// Creates an anonymous pipe.
extern errno_t Process_CreatePipe(ProcessRef _Nonnull pProc, int* _Nonnull pOutReadChannel, int* _Nonnull pOutWriteChannel);

// Returns information about the file at the given path.
extern errno_t Process_GetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileInfo* _Nonnull pOutInfo);

// Same as above but with respect to the given I/O channel.
extern errno_t Process_GetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int fd, FileInfo* _Nonnull pOutInfo);

// Modifies information about the file at the given path.
extern errno_t Process_SetFileInfo(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, MutableFileInfo* _Nonnull pInfo);

// Same as above but with respect to the given I/O channel.
extern errno_t Process_SetFileInfoFromIOChannel(ProcessRef _Nonnull pProc, int fd, MutableFileInfo* _Nonnull pInfo);

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
extern errno_t Process_TruncateFile(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, FileOffset length);

// Same as above but the file is identified by the given I/O channel.
extern errno_t Process_TruncateFileFromIOChannel(ProcessRef _Nonnull pProc, int fd, FileOffset length);

// Sends a I/O Channel or I/O Resource defined command to the I/O Channel or
// resource identified by the given descriptor.
extern errno_t Process_vIOControl(ProcessRef _Nonnull pProc, int fd, int cmd, va_list ap);

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
extern errno_t Process_CheckFileAccess(ProcessRef _Nonnull pProc, const char* _Nonnull pPath, AccessMode mode);

// Unlinks the inode at the path 'pPath'.
extern errno_t Process_Unlink(ProcessRef _Nonnull pProc, const char* _Nonnull pPath);

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
extern errno_t Process_Rename(ProcessRef _Nonnull pProc, const char* pOldPath, const char* pNewPath);

#endif /* Process_h */
