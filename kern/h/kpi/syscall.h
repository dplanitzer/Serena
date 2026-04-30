//
//  kpi/syscall.h
//  kpi
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
    SC_fd_read = 0,         // errno_t fd_read(int fd, const char * _Nonnull buffer, size_t nBytesToRead, ssize_t* pOutBytesRead)
    SC_fd_write,            // errno_t fd_write(int fd, const char * _Nonnull buffer, size_t nBytesToWrite, ssize_t* pOutBytesWritten)
    SC_clock_wait,          // errno_t clock_wait(clockid_t clock, int flags, const nanotime_t* _Nonnull wtp, nanotime_t* _Nullable rmtp)
    SC_vm_allocate,         // errno_t vm_allocate(size_t nbytes, void **pOutMem)
    SC_proc_exit,           // _Noreturn void proc_exit(int code)
    SC_proc_spawn,          // errno_t proc_spawn(const char* _Nonnull path, const char* _Nullable argv[], , const char* _Nullable envp[], proc_spawnattr_t * _Nonnull options, const proc_spawn_actions_t* _Nullable actions, proc_spawnres_t * _Nullable result)
    SC_proc_terminate,      // errno_t proc_terminate(pid_t pid)
    SC_proc_vcpus,          // int proc_vcpus(const vcpu_matcher_t* _Nullable matchers, vcpuid_t* _Nonnull buf, size_t bufSize)
    SC_proc_suspend,        // errno_t proc_suspend(pid_t pid)
    SC_fs_open,             // errno_t fs_open(int wd, const char * _Nonnull path, int oflags, int* _Nonnull fd)
    SC_fd_close,            // errno_t fd_close(int fd)
    SC_proc_status,         // errno_t proc_status(int match, pid_t id, int flags, proc_status_t* _Nonnull status)
    SC_fd_seek,             // errno_t fd_seek(int fd, off_t offset, off_t * _Nullable newpos, int whence)
    SC_proc_property,       // errno_t proc_property(pid_t pid, int flavor, void* _Nonnull buf, size_t bufSize)
    SC_proc_setcwd,         // errno_t proc_setcwd(const char* _Nonnull path)
    SC_host_processes,      // int host_processes(const proc_matcher_t* _Nullable matchers, pid_t* _Nonnull buf, size_t bufSize)
    SC_proc_setumask,       // void proc_setumask(fs_perms_t umask)
    SC_fs_create_directory, // errno_t fs_create_directory(int wd, const char* _Nonnull path, fs_perms_t fsperms)
    SC_fs_attr,             // errno_t fs_attr(int wd, const char* _Nonnull path, fs_attr_t* _Nonnull attr)
    SC_fs_open_directory,   // errno_t fs_open_directory(int wd, const char* _Nonnull path, int* _Nonnull fd)
    SC_fs_access,           // errno_t fs_access(int wd, const char* _Nonnull path, int mode)
    SC_fd_attr,             // errno_t fd_attr(int fd, fs_attr_t* _Nonnull attr)
    SC_fs_unlink,           // errno_t __fs_unlink(int wd, const char* path, int mode)
    SC_fs_rename,           // errno_t fs_rename(int owd, const char* _Nonnull oldpath, int nwd, const char* _Nonnull newpath)
    SC_ioctl,               // errno_t ioctl(int fd, int cmd, ...)
    SC_fs_truncate,         // errno_t fs_truncate(int wd, const char* _Nonnull path, off_t length)
    SC_fd_truncate,         // errno_t fd_truncate(int fd, off_t length)
    SC_fs_create_file,      // errno_t fs_create_file(int wd, const char* _Nonnull path, int options, fs_perms_t fsperms, int* _Nonnull fd)
    SC_pipe_create,         // errno_t pipe_create(int fds[2])
    SC_wq_dispose,          // void wq_dispose(int q)
    SC_clock_time,          // errno_t clock_time(clockid_t clock, nanotime_t* _Nonnull ts)
    SC_fs_mount,            // errno_t fs_mount(const char* _Nonnull objectType, const char* _Nonnull objectName, const char* _Nonnull atDirPath, const char* _Nonnull params)
    SC_fs_unmount,          // errno_t fs_unmount(const char* _Nonnull atDirPath, int flags)
    SC_host_info,           // errno_t host_info(int flavor, host_info_ref _Nonnull info)
    SC_fs_sync,             // void fs_sync(void)
    SC_coninit,             // void __con_init(void)
    SC_fs_property,         // errno_t fs_property(fsid_t fsid, int flavor, char* _Nonnull buf, size_t bufSize)
    SC_vcpu_errno,          // errno_t* _Nonnull __vcpu_errno(void)
    SC_fs_setowner,         // errno_t fs_setowner(int wd, const char* _Nonnull path, uid_t uid, gid_t gid)
    SC_fd_setflags,         // errno_t fd_setflags(int fd, int op, int flags)
    SC_fs_setperms,         // errno_t fs_setperms(int, const char* _Nonnull path, fs_perms_t fsperms)
    SC_fs_settimes,         // errno_t fs_settimes(int, const char* _Nonnull path, const nanotime_t times[_Nullable 2])
    SC_vcpu_yield,          // void vcpu_yield(void)
    SC_wq_create,           // int wq_create(int policy)
    SC_wq_wait,             // int wq_wait(int q)
    SC_wq_timedwait,        // int wq_timedwait(int q, int flags, const nanotime_t* _Nonnull wtp)
    SC_wq_wakeup,           // int wq_wakeup(int q, int flags)
    SC_proc_info,           // int proc_info(pid_t id, int flavor, proc_info_ref _Nonnull info)
    SC_sig_route,           // int sig_route(int op, int signo, int scope, id_t id)
    SC_vcpu_getdata,        // intptr_t __vcpu_getdata(void)
    SC_vcpu_setdata,        // void __vcpu_setdata(intptr_t data)
    SC_sig_wait,            // int sig_wait(const sigset_t* _Nonnull set, int* _Nonnull signo)
    SC_sig_timedwait,       // int sig_timedwait(const sigset_t* _Nonnull set, int flags, const nanotime_t* _Nonnull wtp, int* _Nonnull signo)
    SC_wq_wakeup_then_timedwait,    // int wq_wakeup_then_timedwait(int q, int q2, int flags, const nanotime_t* _Nonnull wtp)
    SC_sig_pending,         // int sig_pending(sigset_t* _Nonnull set)
    SC_host_filesystems,    // errno_t host_filesystems(fsid_t* _Nonnull buf, size_t bufSize)
    SC_fs_info,             // errno_t fs_info(int flavor, fs_info_ref _Nonnull info)
    SC_fd_dup_to,           // errno_t fd_dup_to(int fd, int target_fd, int* _Nonnull new_fd)
    SC_vcpu_acquire,        // int vcpu_acquire(const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp)
    SC_vcpu_relinquish_self,    // void SC_vcpu_relinquish_self(void)
    SC_vcpu_suspend,        // int vcpu_suspend(vcpuid_t vcpu)
    SC_vcpu_resume,         // int vcpu_resume(vcpuid_t vcpu)
    SC_sig_send,            // sig_send(int scope, id_t id, int signo)
    SC_sig_urgent,          // void sig_urgent(void)
    SC_excpt_sethandler,    // int excpt_sethandler(int scope, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
    SC_proc_exec,           // int proc_exec(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable * _Nullable envp)
    SC_vcpu_policy,         // int vcpu_policy(vcpuid_t id, int version, vcpu_policy_t* _Nonnull policy)
    SC_vcpu_setpolicy,      // int vcpu_setpolicy(vcpuid_t id, const vcpu_policy_t* _Nonnull policy)
    SC_clock_info,          // errno_t clock_info(clockid_t clockid, int flavor, clock_info_ref _Nonnull info)
    SC_vcpu_info,           // int vcpu_info(vcpuid_t id, int flavor, vcpu_info_ref _Nonnull info)
    SC_nullsys,             // errno_t __nullsys(void)
    SC_proc_schedparam,     // int proc_schedparam(pid_t pid, int type, int* _Nonnull param)
    SC_proc_setschedparam,  // int proc_setschedparam(pid_t pid, int type, const int* _Nonnull param)
    SC_vcpu_state,          // int vcpu_state(vcpuid_t id, int flavor, vcpu_state_ref _Nonnull state)
    SC_vcpu_setstate,       // int vcpu_setstate(vcpuid_t id, int flavor, const vcpu_state_ref _Nonnull state)
    SC_fs_setlabel,         // errno_t fs_setlabel(fsid_t fsid, const char* _Nonnull label)
    SC_proc_resume,         // errno_t proc_resume(pid_t pid)
    SC_fd_info,             // errno_t fd_info(int fd, int flavor, fd_info_ref _Nonnull info)
    SC_fd_dup,              // errno_t fd_dup(int fd, int min_fd, int* _Nonnull new_fd)
    SC_proc_self,           // pid_t proc_self(void)
};


extern intptr_t _syscall(int scno, ...);

__CPP_END

#endif /* _KPI_SYSCALL_H */
