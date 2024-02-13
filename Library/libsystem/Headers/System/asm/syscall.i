;
;  syscall.i
;  libsystem
;
;  Created by Dietmar Planitzer on 9/6/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

        ifnd __ABI_SYSCALL_I
__ABI_SYSCALL_I  set 1

SC_read                 equ 0   ; errno_t IOChannel_Read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
SC_write                equ 1   ; errno_t IOChannel_Write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
SC_delay                equ 2   ; errno_t Delay(TimeInterval ti)
SC_dispatch_async       equ 3   ; errno_t DispatchQueue_Async(Dispatch_Closure _Nonnull pUserClosure)
SC_alloc_address_space  equ 4   ; errno_t Process_AllocateAddressSpace(int nbytes, void **pOutMem)
SC_exit                 equ 5   ; _Noreturn Process_Exit(int status)
SC_spawn_process        equ 6   ; errno_t Process_Spawn(SpawnArguments * _Nonnull args, ProcessId * _Nullable rpid)
SC_getpid               equ 7   ; ProcessId Process_GetId(void)
SC_getppid              equ 8   ; ProcessId Process_GetParentId(void)
SC_getpargs             equ 9   ; ProcessArguments* _Nonnull Process_GetArguments(void)
SC_open                 equ 10  ; errno_t File_Open(const char * _Nonnull name, int options, int* _Nonnull fd)
SC_close                equ 11  ; errno_t IOChannel_Close(int fd)
SC_waitpid              equ 12  ; errno_t Process_WaitForChildTermination(ProcessId pid, ProcessTerminationStatus * _Nullable result)
SC_seek                 equ 13  ; errno_t File_Seek(int fd, FileOffset offset, FileOffset *newpos, int whence)
SC_getcwd               equ 14  ; errno_t Process_GetCurrentWorkingDirectoryPath(char* buffer, size_t bufferSize)
SC_setcwd               equ 15  ; errno_t Process_SetCurrentWorkingDirectoryPath(const char* path)
SC_getuid               equ 16  ; UserId Process_GetUserId(void)
SC_getumask             equ 17  ; FilePermissions Process_GetUserMask(void)
SC_setumask             equ 18  ; void Process_SetUserMask(FilePermissions mask)
SC_mkdir                equ 19  ; errno_t Directory_Create(const char* path, FilePermissions mode)
SC_getfileinfo          equ 20  ; errno_t File_GetInfo(const char* path, FileInfo* info)
SC_opendir              equ 21  ; errno_t Directory_Open(const char* path, int* fd)
SC_setfileinfo          equ 22  ; errno_t File_SetInfo(const char* path, MutableFileInfo* info)
SC_access               equ 23  ; errno_t File_CheckAccess(const char* path, int mode)
SC_fgetfileinfo         equ 24  ; errno_t FileChannel_GetInfo(int fd, FileInfo* info)
SC_fsetfileinfo         equ 25  ; errno_t FileChannel_SetInfo(int fd, MutableFileInfo* info)
SC_unlink               equ 26  ; errno_t File_Unlink(const char* path)
SC_rename               equ 27  ; errno_t rename(const char* oldpath, const char* newpath)
SC_ioctl                equ 28  ; errno_t IOChannel_Control(int fd, int cmd, ...)
SC_truncate             equ 29  ; errno_t File_Truncate(const char* path, FileOffset length)
SC_ftruncate            equ 30  ; errno_t FileChannel_Truncate(int fd, FileOffset length)
SC_mkfile               equ 31  ; errno_t File_Create(const char* _Nonnull path, int options, int permissions, int* _Nonnull fd)

SC_numberOfCalls        equ 32   ; number of system calls


; System call macro.
;
; A system call looks like this:
;
; a0.l: -> pointer to argument list base
;
; d0.l: <- error number
;
; Register a0 holds a pointer to the base of the argument list. Arguments are
; expected to be ordered from left to right (same as the standard C function
; call ABI) and the pointer in a0 points to the left-most argument. So the
; simplest way to pass arguments to a system call is to push them on the user
; stack starting with the right-most argument and ending with the left-most
; argument and to then initialize a0 like this:
;
; move.l sp, a0
;
; If the arguments are first put on the stack and you then call a subroutine
; which does the actual trap #0 to the kernel, then you want to initialize a0
; like this:
;
; lea 4(sp), a0
;
; since the user stack pointer points to the return address on the stack and not
; the system call number.
;
; The system call returns the error code in d0.
;
    macro SYSCALL
    trap    #0
    endm

        endif   ; __ABI_SYSCALL_I
