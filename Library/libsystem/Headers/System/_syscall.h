//
//  syscall.h
//  libsystem
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <System/_cmndef.h>
#include <System/abi/_inttypes.h>

__CPP_BEGIN

enum {
    SC_read = 0,            // errno_t read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
    SC_write,               // errno_t write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
    SC_clock_wait,          // errno_t clock_wait(int clock, const TimeInterval* _Nonnull delay)
    SC_dispatch,            // errno_t _DispatchQueue_Dispatch(int od, Dispatch_Closure _Nonnull func, void* _Nullable ctx, uint32_t options, uintptr_t tag)
    SC_alloc_address_space, // errno_t Process_AllocateAddressSpace(int nbytes, void **pOutMem)
    SC_exit,                // _Noreturn Process_Exit(int status)
    SC_spawn_process,       // errno_t Process_Spawn(cost char* _Nonnull path, const char* _Nullable argv[], os_spawn_opts_t * _Nullable options, pid_t * _Nullable rpid)
    SC_getpid,              // pid_t Process_GetId(void)
    SC_getppid,             // pid_t Process_GetParentId(void)
    SC_getpargs,            // os_procargs_t * _Nonnull Process_GetArguments(void)
    SC_open,                // errno_t open(const char * _Nonnull name, int options, int* _Nonnull fd)
    SC_close,               // errno_t close(int fd)
    SC_waitpid,             // errno_t Process_WaitForChildTermination(pid_t pid, os_proc_status_t * _Nullable result)
    SC_seek,                // errno_t seek(int fd, off_t offset, off_t * _Nullable newpos, int whence)
    SC_getcwd,              // errno_t Process_GetWorkingDirectory(char* buffer, size_t bufferSize)
    SC_setcwd,              // errno_t Process_SetWorkingDirectory(const char* _Nonnull path)
    SC_getuid,              // uid_t Process_GetUserId(void)
    SC_getumask,            // FilePermissions Process_GetUserMask(void)
    SC_setumask,            // void Process_SetUserMask(FilePermissions mask)
    SC_mkdir,               // errno_t mkdir(const char* _Nonnull path, FilePermissions mode)
    SC_getinfo,             // errno_t getinfo(const char* _Nonnull path, FileInfo* _Nonnull info)
    SC_opendir,             // errno_t opendir(const char* _Nonnull path, int* _Nonnull fd)
    SC_setinfo,             // errno_t setinfo(const char* _Nonnull path, MutableFileInfo* _Nonnull info)
    SC_access,              // errno_t access(const char* _Nonnull path, int mode)
    SC_fgetinfo,            // errno_t fgetinfo(int fd, FileInfo* _Nonnull info)
    SC_fsetinfo,            // errno_t fsetinfo(int fd, MutableFileInfo* _Nonnull info)
    SC_unlink,              // errno_t unlink(const char* path)
    SC_rename,              // errno_t rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
    SC_ioctl,               // errno_t fcall(int fd, int cmd, ...)
    SC_truncate,            // errno_t truncate(const char* _Nonnull path, off_t length)
    SC_ftruncate,           // errno_t ftruncate(int fd, off_t length)
    SC_mkfile,              // errno_t mkfile(const char* _Nonnull path, int options, int permissions, int* _Nonnull fd)
    SC_mkpipe,              // errno_t pipe_create(int* _Nonnull rioc, int* _Nonnull wioc)
    SC_dispatch_timer,      // errno_t DispatchQueue_DispatchTimer(int od, TimeInterval deadline, TimeInterval interval, Dispatch_Closure _Nonnull func, void* _Nullable ctx, uintptr_t tag)
    SC_dispatch_queue_create,   // errno_t DispatchQueue_Create(int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
    SC_dispatch_queue_current,  // int DispatchQueue_GetCurrent(void)
    SC_dispose,             // _Object_Dispose(int od)
    SC_clock_gettime,       // errno_t clock_gettime(int clock, TimeInterval* _Nonnull ts)
    SC_lock_create,         // errno_t lock_create(int* _Nonnull pOutOd)
    SC_lock_trylock,        // errno_t lock_trylock(int od)
    SC_lock_lock,           // errno_t lock_lock(int od)
    SC_lock_unlock,         // errno_t lock_unlock(int od)
    SC_sem_create,          // errno_t sem_create(int npermits, int* _Nonnull pOutOd)
    SC_sem_post,            // errno_t sem_post(int od, int npermits)
    SC_sem_wait,            // errno_t sem_wait(int od, int npermits, TimeInterval deadline)
    SC_sem_trywait,         // errno_t sem_trywait(int od, int npermits)
    SC_cond_create,         // errno_t cond_create(int* _Nonnull pOutOd)
    SC_cond_wake,           // errno_t cond_wake(int od, int dlock, unsigned int options)
    SC_cond_timedwait,      // errno_t cond_timedwait(int od, int dlock, const TimeInterval* _Nonnull deadline)
    SC_dispatch_remove_by_tag,  // bool dispatch_remove_by_tag(int od, uintptr_t tag)
    SC_mount,               // errno_t mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
    SC_unmount,             // errno_t unmount(const char* _Nonnull atDirPath, UnmountOptions options)
    SC_getgid,              // gid_t Process_GetGroupId(void)
    SC_sync,                // void sync(void)
    SC_coninit,             // void ConInit(void)
    SC_fsgetdisk,           // errno_t fsgetdisk(fsid_t fsid, char* _Nonnull buf, size_t bufSize)
};


extern intptr_t _syscall(int scno, ...);

__CPP_END

#endif /* _SYS_SYSCALL_H */
