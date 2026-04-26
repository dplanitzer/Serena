//
//  kpi/spawn.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SPAWN_H
#define _KPI_SPAWN_H 1

#include <_cmndef.h>
#include <kpi/types.h>

// Instructs the proc_spawn() call to set the umask of the newly spawned
// process to the umask field in the spawn arguments struct rather than the
// umask field of the parent process.
#define PROC_SPAWN_UMASK            0x0001

// The new process should use the provided user id rather than the parent process
// user id. Parent process must be the superuser (XXX for now).
#define PROC_SPAWN_UID              0x0002

// The new process should use the provided group id rather than the parent process
// group id. Parent process must be the superuser (XXX for now).
#define PROC_SPAWN_GID              0x0004

// A new process group should be created with the new process being the group
// leader. The id of the new group will be equal to the pid of the new process.
#define PROC_SPAWN_GROUP_LEADER     0x0010

// A new session should be created with the new process being the session leader.
// The id of the new session will be equal to the pid of the new process.
#define PROC_SPAWN_SESSION_LEADER   0x0020


typedef struct proc_spawn {
    const char* _Nullable   root_dir;               // Process root directory, if not NULL; otherwise inherited from the parent
    const char* _Nullable   cw_dir;                 // Process current working directory, if not NULL; otherwise inherited from the parent
    fs_perms_t              umask;                  // Override umask
    uid_t                   uid;                    // Override user ID
    gid_t                   gid;                    // Override group ID
    unsigned int            options;
} proc_spawn_t;

#endif /* _KPI_SPAWN_H */
