//
//  kpi/syscall.h
//  libc
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SYSCALL_H
#define _KPI_SYSCALL_H 1

#include <_cmndef.h>
#include <stdint.h>

__CPP_BEGIN

enum {
    SC_read = 0,            // errno_t read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
    SC_write,               // errno_t write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
    SC_clock_nanosleep,     // errno_t clock_nanosleep(clockid_t clock, int flags, const struct timespec* _Nonnull wtp, struct timespec* _Nullable rmtp)
    SC_vmalloc,             // errno_t vmalloc(int nbytes, void **pOutMem)
    SC_exit,                // _Noreturn exit(int status)
    SC_spawn,               // errno_t spawn(cost char* _Nonnull path, const char* _Nullable argv[], spawn_opts_t * _Nullable options, pid_t * _Nullable rpid)
    SC_getpid,              // pid_t getpid(void)
    SC_getppid,             // pid_t getppid(void)
    SC_getpargs,            // pargs_t * _Nonnull getpargs(void)
    SC_open,                // errno_t open(const char * _Nonnull name, int options, int* _Nonnull fd)
    SC_close,               // errno_t close(int fd)
    SC_proc_join,           // int proc_timedjoin(int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps)
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
    SC_ioctl,               // errno_t ioctl(int fd, int cmd, ...)
    SC_truncate,            // errno_t truncate(const char* _Nonnull path, off_t length)
    SC_ftruncate,           // errno_t ftruncate(int fd, off_t length)
    SC_mkfile,              // errno_t mkfile(const char* _Nonnull path, int options, int permissions, int* _Nonnull fd)
    SC_pipe,                // errno_t pipe(int* _Nonnull rioc, int* _Nonnull wioc)
    SC_wq_dispose,          // void wq_dispose(int q)
    SC_clock_gettime,       // errno_t clock_gettime(clockid_t clock, struct timespec* _Nonnull ts)
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
    SC_vcpu_yield,          // void vcpu_yield(void)
    SC_wq_create,           // int wq_create(int policy)
    SC_wq_wait,             // int wq_wait(int q)
    SC_wq_timedwait,        // int wq_timedwait(int q, int flags, const struct timespec* _Nonnull wtp)
    SC_wq_wakeup,           // int wq_wakeup(int q, int flags)
    SC_vcpu_getid,          // int vcpu_getid(void)
    SC_sigroute,            // int sigroute(int scope, id_t id, int op)
    SC_vcpu_getdata,        // intptr_t __vcpu_getdata(void)
    SC_vcpu_setdata,        // void __vcpu_setdata(intptr_t data)
    SC_sigwait,             // int sigwait(const sigset_t* _Nonnull set, int* _Nonnull signo)
    SC_sigtimedwait,        // int sigtimedwait(const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo)
    SC_wq_wakeup_then_timedwait,    // int wq_wakeup_then_timedwait(int q, int q2, int flags, const struct timespec* _Nonnull wtp)
    SC_sigpending,          // int sigpending(sigset_t* _Nonnull set)
    SC_vcpu_getgrp,         // int vcpu_getgrp(void)
    SC_getpgrp,             // pid_t getpgrp(void)
    SC_getsid,              // sid_t getsid(void)
    SC_vcpu_acquire,        // int vcpu_acquire(const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp)
    SC_vcpu_relinquish_self,    // void SC_vcpu_relinquish_self(void)
    SC_vcpu_suspend,        // int vcpu_suspend(vcpuid_t vcpu)
    SC_vcpu_resume,         // int vcpu_resume(vcpuid_t vcpu)
    SC_sigsend,             // sigsend(int scope, id_t id, int signo)
    SC_sigurgent,           // void sigurgent(void)
};


extern intptr_t _syscall(int scno, ...);

__CPP_END

#endif /* _KPI_SYSCALL_H */
