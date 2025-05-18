//
//  Process.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Process_h
#define Process_h

#include <stdarg.h>
#include <kern/types.h>
#include <kobj/Object.h>
#include <kpi/mount.h>
#include <kpi/proc.h>
#include <kpi/spawn.h>
#include <kpi/stat.h>
#include <kpi/uid.h>
#include <kpi/wait.h>

final_class(Process, Object);


extern ProcessRef _Nonnull  gRootProcess;


// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
extern ProcessRef _Nullable Process_GetCurrent(void);


// Creates the root process which is the first process of the OS.
extern errno_t RootProcess_Create(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutProc);


// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
extern void Process_Terminate(ProcessRef _Nonnull self, int exitCode);

// Returns true if the process is marked for termination and false otherwise.
extern bool Process_IsTerminating(ProcessRef _Nonnull self);

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if there are no tombstones of terminated
// child processes available or the PID is not the PID of a child process of
// the receiver. Otherwise blocks the caller until the requested process or any
// child process (pid == -1) has exited.
extern errno_t Process_WaitForTerminationOfChild(ProcessRef _Nonnull self, pid_t pid, pstatus_t* _Nullable pStatus);

extern int Process_GetId(ProcessRef _Nonnull self);
extern int Process_GetParentId(ProcessRef _Nonnull self);

extern uid_t Process_GetRealUserId(ProcessRef _Nonnull self);
extern gid_t Process_GetRealGroupId(ProcessRef _Nonnull self);

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
extern void* _Nonnull Process_GetArgumentsBaseAddress(ProcessRef _Nonnull self);

// Spawns a new process that will be a child of the given process. The spawn
// arguments specify how the child process should be created, which arguments
// and environment it will receive and which descriptors it will inherit.
extern errno_t Process_SpawnChildProcess(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull opts, pid_t * _Nullable pOutChildPid);

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param self the process into which the executable image should be loaded
// \param pExecPath path to a GemDOS executable file
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
extern errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[]);


// Disposes the user resource identified by the given descriptor. The resource
// is deallocated and removed from the resource table.
extern errno_t Process_DisposeUResource(ProcessRef _Nonnull self, int od);


// Dispatches the execution of the given user closure on the given dispatch queue
// with the given options. 
extern errno_t Process_DispatchUserClosure(ProcessRef _Nonnull self, int od, VoidFunc_2 _Nonnull func, void* _Nullable ctx, uint32_t options, uintptr_t tag);

// Dispatches the execution of the given user closure on the given dispatch queue
// after the given deadline.
extern errno_t Process_DispatchUserTimer(ProcessRef _Nonnull self, int od, struct timespec deadline, struct timespec interval, VoidFunc_1 _Nonnull func, void* _Nullable ctx, uintptr_t tag);

extern errno_t Process_DispatchRemoveByTag(ProcessRef _Nonnull self, int od, uintptr_t tag);

// Returns the dispatch queue associated with the virtual processor on which the
// calling code is running. Note this function assumes that it will ALWAYS be
// called from a system call context and thus the caller will necessarily run in
// the context of a (process owned) dispatch queue.
extern int Process_GetCurrentDispatchQueue(ProcessRef _Nonnull self);

// Creates a new dispatch queue and binds it to the process.
extern errno_t Process_CreateDispatchQueue(ProcessRef _Nonnull self, int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nullable pOutDescriptor);


// Creates a new UConditionVariable and binds it to the process.
extern errno_t Process_CreateUConditionVariable(ProcessRef _Nonnull self, int* _Nullable pOutOd);

// Wakes the given condition variable and unlock the associated lock if
// 'dLock' is not -1. This does a signal or broadcast.
extern errno_t Process_WakeUConditionVariable(ProcessRef _Nonnull self, int od, int dLock, bool bBroadcast);

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Automatically and atomically acquires the associated
// lock on wakeup. An ETIMEOUT error is returned if the condition variable is
// not signaled before 'deadline'.
extern errno_t Process_WaitUConditionVariable(ProcessRef _Nonnull self, int od, int dLock, struct timespec deadline);


// Creates a new ULock and binds it to the process.
extern errno_t Process_CreateULock(ProcessRef _Nonnull self, int* _Nullable pOutLock);

// Tries taking the given lock. Returns EOK on success and EBUSY if someone else
// is already holding the lock.
extern errno_t Process_TryULock(ProcessRef _Nonnull self, int od);

// Locks the given user lock. The caller will remain blocked until the lock can
// be successfully acquired or the wait is interrupted for some reason.
extern errno_t Process_LockULock(ProcessRef _Nonnull self, int od);

// Unlocks the given user lock. Returns EOK on success and EPERM if the lock is
// currently being held by some other virtual processor.
extern errno_t Process_UnlockULock(ProcessRef _Nonnull self, int od);


// Creates a new USemaphore and binds it to the process.
extern errno_t Process_CreateUSemaphore(ProcessRef _Nonnull self, int npermits, int* _Nullable pOutOd);

// Releases 'npermits' permits to the semaphore.
extern errno_t Process_RelinquishUSemaphore(ProcessRef _Nonnull self, int od, int npermits);

// Blocks the caller until 'npermits' can be successfully acquired from the given
// semaphore. Returns EOK on success and ETIMEOUT if the permits could not be
// acquired before 'deadline'.
extern errno_t Process_AcquireUSemaphore(ProcessRef _Nonnull self, int od, int npermits, struct timespec deadline);

// Tries to acquire 'npermits' from the given semaphore. Returns true on success
// and false otherwise. This function does not block the caller.
extern errno_t Process_TryAcquireUSemaphore(ProcessRef _Nonnull self, int npermits, int od);


// Allocates more (user) address space to the given process.
extern errno_t Process_AllocateAddressSpace(ProcessRef _Nonnull self, ssize_t count, void* _Nullable * _Nonnull pOutMem);


//
// I/O Channels
//

extern errno_t Process_CloseChannel(ProcessRef _Nonnull self, int fd);

extern errno_t Process_ReadChannel(ProcessRef _Nonnull self, int fd, void* _Nonnull buffer, size_t nBytesToRead, ssize_t* _Nonnull nBytesRead);

extern errno_t Process_WriteChannel(ProcessRef _Nonnull self, int fd, const void* _Nonnull buffer, size_t nBytesToWrite, ssize_t* _Nonnull nBytesWritten);

extern errno_t Process_SeekChannel(ProcessRef _Nonnull self, int fd, off_t offset, off_t* _Nullable pOutOldPosition, int whence);

extern int Process_Fcntl(ProcessRef _Nonnull self, int fd, int cmd, int* _Nonnull pResult, va_list ap);

extern errno_t Process_Iocall(ProcessRef _Nonnull self, int fd, int cmd, va_list ap);


//
// Directories
//

// Sets the receiver's root directory to the given path. Note that the path must
// point to a directory that is a child or the current root directory of the
// process.
extern errno_t Process_SetRootDirectoryPath(ProcessRef _Nonnull self, const char* pPath);

// Sets the receiver's current working directory to the given path.
extern errno_t Process_SetWorkingDirectoryPath(ProcessRef _Nonnull self, const char* _Nonnull pPath);

// Returns the current working directory in the form of a path. The path is
// written to the provided buffer 'pBuffer'. The buffer size must be at least as
// large as length(path) + 1.
extern errno_t Process_GetWorkingDirectoryPath(ProcessRef _Nonnull self, char* _Nonnull pBuffer, size_t bufferSize);


//
// Files
//

// Sets the umask. Bits set in 'mask' are cleared in the mode used to create a
// file. Returns the old umask.
extern mode_t Process_UMask(ProcessRef _Nonnull self, mode_t mask);

// Creates a file in the given filesystem location.
extern errno_t Process_CreateFile(ProcessRef _Nonnull self, const char* _Nonnull pPath, unsigned int mode, mode_t permissions, int* _Nonnull pOutDescriptor);

// Opens the given file or named resource. Opening directories is handled by the
// Process_OpenDirectory() function.
extern errno_t Process_OpenFile(ProcessRef _Nonnull self, const char* _Nonnull pPath, unsigned int mode, int* _Nonnull pOutDescriptor);

// Creates a new directory. 'permissions' are the file permissions that should be
// assigned to the new directory (modulo the file creation mask).
extern errno_t Process_CreateDirectory(ProcessRef _Nonnull self, const char* _Nonnull pPath, mode_t permissions);

// Opens the directory at the given path and returns an I/O channel that represents
// the open directory.
extern errno_t Process_OpenDirectory(ProcessRef _Nonnull self, const char* _Nonnull pPath, int* _Nonnull pOutDescriptor);

// Creates an anonymous pipe.
extern errno_t Process_CreatePipe(ProcessRef _Nonnull self, int* _Nonnull pOutReadChannel, int* _Nonnull pOutWriteChannel);

// Returns information about the file at the given path.
extern errno_t Process_GetFileInfo(ProcessRef _Nonnull self, const char* _Nonnull pPath, finfo_t* _Nonnull pOutInfo);

// Same as above but with respect to the given I/O channel.
extern errno_t Process_GetFileInfo_ioc(ProcessRef _Nonnull self, int fd, finfo_t* _Nonnull pOutInfo);

extern errno_t Process_SetFileMode(ProcessRef _Nonnull self, const char* _Nonnull path, mode_t mode);

extern errno_t Process_SetFileOwner(ProcessRef _Nonnull self, const char* _Nonnull path, uid_t uid, gid_t gid);

extern errno_t Process_SetFileTimestamps(ProcessRef _Nonnull self, const char* _Nonnull path, const struct timespec times[_Nullable 2]);

// Sets the length of an existing file. The file may either be reduced in size
// or expanded.
extern errno_t Process_TruncateFile(ProcessRef _Nonnull self, const char* _Nonnull pPath, off_t length);

// Same as above but the file is identified by the given I/O channel.
extern errno_t Process_TruncateFile_ioc(ProcessRef _Nonnull self, int fd, off_t length);

// Returns EOK if the given file is accessible assuming the given access mode;
// returns a suitable error otherwise. If the mode is 0, then a check whether the
// file exists at all is executed.
extern errno_t Process_CheckAccess(ProcessRef _Nonnull self, const char* _Nonnull pPath, int mode);

// Unlinks the inode at the path 'pPath'.
extern errno_t Process_Unlink(ProcessRef _Nonnull self, const char* _Nonnull pPath);

// Renames the file or directory at 'pOldPath' to the new location 'pNewPath'.
extern errno_t Process_Rename(ProcessRef _Nonnull self, const char* pOldPath, const char* pNewPath);


//
// Filesystems
//

extern errno_t Process_Mount(ProcessRef _Nonnull self, const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params);

extern errno_t Process_Unmount(ProcessRef _Nonnull self, const char* _Nonnull atDirPath, UnmountOptions options);

extern errno_t Process_GetFilesystemDiskPath(ProcessRef _Nonnull self, fsid_t fsid, char* _Nonnull buf, size_t bufSize);


//
// Introspection
//

extern errno_t Process_Open(ProcessRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);
extern errno_t Process_Close(ProcessRef _Nonnull self, IOChannelRef _Nonnull chan);


extern errno_t Process_GetInfo(ProcessRef _Nonnull self, procinfo_t* _Nonnull info);
extern errno_t Process_GetName(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize);

extern errno_t Process_Ioctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, ...);
extern errno_t Process_vIoctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap);

#endif /* Process_h */
