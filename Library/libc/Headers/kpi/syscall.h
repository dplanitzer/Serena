//
//  syscall.h
//  libc
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H 1

#include <_cmndef.h>
#include <stdint.h>

__CPP_BEGIN

enum {
    SC_read = 0,            // errno_t read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
    SC_write,               // errno_t write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
    SC_clock_nanosleep,     // errno_t clock_nanosleep(clockid_t clock, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
    SC_disp_schedule,       // errno_t disp_schedule(int od, dispatch_func_t _Nonnull func, void* _Nullable ctx, uint32_t options, uintptr_t tag)
    SC_vmalloc,             // errno_t vmalloc(int nbytes, void **pOutMem)
    SC_exit,                // _Noreturn exit(int status)
    SC_spawn,               // errno_t spawn(cost char* _Nonnull path, const char* _Nullable argv[], spawn_opts_t * _Nullable options, pid_t * _Nullable rpid)
    SC_getpid,              // pid_t getpid(void)
    SC_getppid,             // pid_t getppid(void)
    SC_getpargs,            // pargs_t * _Nonnull getpargs(void)
    SC_open,                // errno_t open(const char * _Nonnull name, int options, int* _Nonnull fd)
    SC_close,               // errno_t close(int fd)
    SC_waitpid,             // errno_t waitpid(pid_t pid, pstatus_t * _Nullable result, int options)
    SC_lseek,               // errno_t lseek(int fd, off_t offset, off_t * _Nullable newpos, int whence)
    SC_getcwd,              // errno_t getcwd(char* buffer, size_t bufferSize)
    SC_chdir,               // errno_t chdir(const char* _Nonnull path)
    SC_getuid,              // uid_t getuid(void)
    SC_umask,               // mode_t umask(mode_t mask)
    SC_mkdir,               // errno_t mkdir(const char* _Nonnull path, mode_t mode)
    SC_stat,                // errno_t stat(const char* _Nonnull path, struct stat* _Nonnull info)
    SC_opendir,             // errno_t opendir(const char* _Nonnull path, int* _Nonnull fd)
    SC_access,              // errno_t access(const char* _Nonnull path, int mode)
    SC_fstat,               // errno_t fstat(int fd, struct stat* _Nonnull info)
    SC_unlink,              // errno_t __unlink(const char* path, int mode)
    SC_rename,              // errno_t rename(const char* _Nonnull oldpath, const char* _Nonnull newpath)
    SC_ioctl,               // errno_t fcall(int fd, int cmd, ...)
    SC_truncate,            // errno_t truncate(const char* _Nonnull path, off_t length)
    SC_ftruncate,           // errno_t ftruncate(int fd, off_t length)
    SC_creat,               // errno_t creat(const char* _Nonnull path, int options, int permissions, int* _Nonnull fd)
    SC_pipe,                // errno_t pipe(int* _Nonnull rioc, int* _Nonnull wioc)
    SC_disp_timer,          // errno_t disp_timer(int od, struct timespec deadline, struct timespec interval, dispatch_func_t _Nonnull func, void* _Nullable ctx, uintptr_t tag)
    SC_disp_create,         // errno_t disp_create(int minConcurrency, int maxConcurrency, int qos, int priority, int* _Nonnull pOutQueue)
    SC_disp_getcurrent,     // int disp_getcurrent(void)
    SC_dispose,             // void dispose(int od)
    SC_clock_gettime,       // errno_t clock_gettime(clockid_t clock, struct timespec* _Nonnull ts)
    SC_disp_removebytag,    // bool disp_removebytag(int od, uintptr_t tag)
    SC_mount,               // errno_t mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
    SC_unmount,             // errno_t unmount(const char* _Nonnull atDirPath, UnmountOptions options)
    SC_getgid,              // gid_t getgid(void)
    SC_sync,                // void sync(void)
    SC_coninit,             // void ConInit(void)
    SC_fsgetdisk,           // errno_t fsgetdisk(fsid_t fsid, char* _Nonnull buf, size_t bufSize)
    SC_vcpu_errno,          // errno_t* _Nonnull __vcpu_errno(void)
    SC_chown,               // errno_t chown(const char* _Nonnull path, uid_t uid, gid_t gid)
    SC_fcntl,               // int fcntl(int fd, int cmd, int* _Nonnull pResult, ...)
    SC_chmod,               // errno_t chmod(const char* _Nonnull path, mode_t mode)
    SC_utimens,             // errno_t utimens(const char* _Nonnull path, const struct timespec times[_Nullable 2])
    SC_sched_yield,         // void sched_yield(void)
    SC_wq_create,           // int wq_create(int policy)
    SC_wq_wait,             // int wq_wait(int q)
    SC_wq_timedwait,        // int wq_timedwait(int q, int options, const struct timespec* _Nonnull wtp)
    SC_wq_wakeup,           // int wq_wakeup(int q, int flags, int signo)
    SC_vcpu_self,           // int vcpu_self(void)
    SC_vcpu_setsigmask,     // int vcpu_setsigmask(int op, sigset_t mask, sigset_t* _Nullable oldmask)
    SC_vcpu_getdata,        // intptr_t __vcpu_getdata(void)
    SC_vcpu_setdata,        // void __vcpu_setdata(intptr_t data)
    SC_wq_sigwait,          // int wq_sigwait(int q, const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs)
    SC_wq_sigtimedwait,     // int wq_sigtimedwait(int q, const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs, int flags, const struct timespec* _Nonnull wtp)
};


extern intptr_t _syscall(int scno, ...);

__CPP_END

#endif /* _SYS_SYSCALL_H */
