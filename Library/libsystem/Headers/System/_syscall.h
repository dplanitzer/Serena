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

__CPP_BEGIN

#define SC_read                 0   // errno_t IOChannel_Read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
#define SC_write                1   // errno_t IOChannel_Write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
#define SC_delay                2   // errno_t Delay(TimeInterval ti)
#define SC_dispatch_async       3   // errno_t DispatchQueue_Async(Dispatch_Closure _Nonnull pUserClosure)
#define SC_alloc_address_space  4   // errno_t Process_AllocateAddressSpace(int nbytes, void **pOutMem)
#define SC_exit                 5   // _Noreturn Process_Exit(int status)
#define SC_spawn_process        6   // errno_t Process_Spawn(SpawnArguments * _Nonnull args, ProcessId * _Nullable rpid)
#define SC_getpid               7   // ProcessId Process_GetId(void)
#define SC_getppid              8   // ProcessId Process_GetParentId(void)
#define SC_getpargs             9   // ProcessArguments * _Nonnull Process_GetArguments(void)
#define SC_open                 10  // errno_t File_Open(const char * _Nonnull name, int options, int* _Nonnull fd)
#define SC_close                11  // errno_t IOChannel_Close(int fd)
#define SC_waitpid              12  // errno_t Process_WaitForChildTermination(ProcessId pid, ProcessTerminationStatus * _Nullable result)
#define SC_seek                 13  // errno_t File_Seek(int fd, FileOffset offset, FileOffset * _Nullable newpos, int whence)
#define SC_getcwd               14  // errno_t Process_GetWorkingDirectory(char* buffer, size_t bufferSize)
#define SC_setcwd               15  // errno_t Process_SetWorkingDirectory(const char* _Nonnull path)
#define SC_getuid               16  // UserId Process_GetUserId(void)
#define SC_getumask             17  // FilePermissions Process_GetUserMask(void)
#define SC_setumask             18  // void Process_SetUserMask(FilePermissions mask)
#define SC_mkdir                19  // errno_t Directory_Create(const char* _Nonnull path, FilePermissions mode)
#define SC_getfileinfo          20  // errno_t File_GetInfo(const char* _Nonnull path, FileInfo* _Nonnull info)
#define SC_opendir              21  // errno_t Directory_Open(const char* _Nonnull path, int* _Nonnull fd)
#define SC_setfileinfo          22  // errno_t File_SetInfo(const char* _Nonnull path, MutableFileInfo* _Nonnull info)
#define SC_access               23  // errno_t File_CheckAccess(const char* _Nonnull path, int mode)
#define SC_fgetfileinfo         24  // errno_t FileChannel_GetInfo(int fd, FileInfo* _Nonnull info)
#define SC_fsetfileinfo         25  // errno_t FileChannel_SetInfo(int fd, MutableFileInfo* _Nonnull info)
#define SC_unlink               26  // errno_t File_Unlink(const char* path)
#define SC_rename               27  // errno_t rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
#define SC_ioctl                28  // errno_t IOChannel_Control(int fd, int cmd, ...)
#define SC_truncate             29  // errno_t File_Truncate(const char* _Nonnull path, FileOffset length)
#define SC_ftruncate            30  // errno_t FileChannel_Truncate(int fd, FileOffset length)
#define SC_mkfile               31  // errno_t File_Create(const char* _Nonnull path, int options, int permissions, int* _Nonnull fd)


extern int _syscall(int scno, ...);

__CPP_END

#endif /* _SYS_SYSCALL_H */
