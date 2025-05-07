//
//  Process.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PROCESS_H
#define _SYS_PROCESS_H 1

#include <System/_cmndef.h>
#include <System/_noreturn.h>
#include <System/Error.h>
#include <System/Types.h>
#include <System/Urt.h>
#include <System/DispatchQueue.h>

__CPP_BEGIN

// The process arguments descriptor is stored in the process address space and
// it contains a pointer to the base of the command line arguments and environment
// variables tables. These tables store pointers to nul-terminated strings and
// the last entry in the table contains a NULL.
// This data structure is set up by the kernel when it processes a Spawn()
// request. Once set up the kernel neither reads nor writes to this area.
typedef struct os_procargs {
    size_t                      version;        // sizeof(os_procargs_t)
    size_t                      reserved;
    size_t                      arguments_size; // Size of the area that holds all of os_procargs_t + argv + envp
    size_t                      argc;           // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  argv;           // Pointer to the base of the command line arguments table. Last entry is NULL
    char* _Nullable * _Nonnull  envp;           // Pointer to the base of the environment table. Last entry holds NULL.
    void* _Nonnull              image_base;     // Pointer to the base of the executable header
    UrtFunc* _Nonnull           urt_funcs;      // Pointer to the URT function table
} os_procargs_t;


// Instructs the Process_Spawn() call to set the umask of the newly spawned
// process to the umask field in the spawn arguments struct rather than the
// umask field of the parent process.
#define kSpawn_OverrideUserMask     0x0001

// The new process should use the provided user id rather than the parent process
// user id. Parent process must be the superuser (XXX for now).
#define kSpawn_OverrideUserId       0x0002

// The new process should use the provided group id rather than the parent process
// group id. Parent process must be the superuser (XXX for now).
#define kSpawn_OverrideGroupId      0x0004

// Tells the kernel that it should notify the parent process when the child
// process terminates for some reason. The parent process must specify a
// dispatch queue and closure.
#define kSpawn_NotifyOnProcessTermination   0x0008


// The 'envp' pointer points to a table of nul-terminated strings of the form
// 'key=value'. The last entry in the table has to be NULL. All these strings
// are the enviornment variables that should be passed to the new process.
// 'envp' may be NULL pointer. A NULL pointer is equivalent to a table with a
// single entry that is the NULL pointer. So a NULL 'envp' means that the child
// process receives an empty environment.
typedef struct os_spawn_opts {
    const char* _Nullable * _Nullable   envp;
    const char* _Nullable               root_dir;               // Process root directory, if not NULL; otherwise inherited from the parent
    const char* _Nullable               cw_dir;                 // Process current working directory, if not NULL; otherwise inherited from the parent
    FilePermissions                     umask;                  // Override umask
    uid_t                               uid;                    // Override user ID
    gid_t                               gid;                    // Override group ID
    int                                 notificationQueue;      // If kSpawn_NotifyOnProcessTermination is set, then this queue will receive termination notifications
    Dispatch_Closure _Nullable          notificationClosure;
    void* _Nullable                     notificationContext;
    uint32_t                            options;
} os_spawn_opts_t;


// The result of a Process_WaitForTerminationOfChild() system call.
typedef struct os_proc_status {
    pid_t   pid;        // PID of the child process
    int     status;     // Child process exit status
} os_proc_status_t;


#if !defined(__KERNEL__)

extern _Noreturn Process_Exit(int exit_code);


extern errno_t Process_GetWorkingDirectory(char* _Nonnull buffer, size_t bufferSize);
extern errno_t Process_SetWorkingDirectory(const char* _Nonnull path);


extern FilePermissions Process_GetUserMask(void);
extern void Process_SetUserMask(FilePermissions mask);


extern pid_t Process_GetId(void);
extern pid_t Process_GetParentId(void);


extern uid_t Process_GetUserId(void);
extern gid_t Process_GetGroupId(void);


extern errno_t Process_Spawn(const char* _Nonnull path, const char* _Nullable argv[], const os_spawn_opts_t* _Nullable options, pid_t* _Nullable rpid);
extern errno_t Process_WaitForTerminationOfChild(pid_t pid, os_proc_status_t* _Nullable result);

extern os_procargs_t* _Nonnull Process_GetArguments(void);


extern errno_t Process_AllocateAddressSpace(size_t nbytes, void* _Nullable * _Nonnull ptr);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_PROCESS_H */
