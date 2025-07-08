//
//  kpi/spawn.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SPAWN_H
#define _KPI_SPAWN_H 1

#include <_cmndef.h>
#include <kpi/stat.h>
#include <kpi/types.h>
#include <kpi/dispatch.h>

// Instructs the os_spawn() call to set the umask of the newly spawned
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

// A new process group should be created with the new process being the group
// leader. The id of the new group will be equal to the pid of the new process.
#define kSpawn_NewProcessGroup  0x0010

// A new session should be created with the new process being the session leader.
// The id of the new session will be equal to the pid of the new process.
#define kSpawn_NewSession       0x0020


// The 'envp' pointer points to a table of nul-terminated strings of the form
// 'key=value'. The last entry in the table has to be NULL. All these strings
// are the enviornment variables that should be passed to the new process.
// 'envp' may be NULL pointer. A NULL pointer is equivalent to a table with a
// single entry that is the NULL pointer. So a NULL 'envp' means that the child
// process receives an empty environment.
typedef struct spawn_opts {
    const char* _Nullable * _Nullable   envp;
    const char* _Nullable               root_dir;               // Process root directory, if not NULL; otherwise inherited from the parent
    const char* _Nullable               cw_dir;                 // Process current working directory, if not NULL; otherwise inherited from the parent
    mode_t                              umask;                  // Override umask
    uid_t                               uid;                    // Override user ID
    gid_t                               gid;                    // Override group ID
    int                                 notificationQueue;      // If kSpawn_NotifyOnProcessTermination is set, then this queue will receive termination notifications
    dispatch_func_t _Nullable           notificationClosure;
    void* _Nullable                     notificationContext;
    unsigned int                        options;
} spawn_opts_t;

#endif /* _KPI_SPAWN_H */
