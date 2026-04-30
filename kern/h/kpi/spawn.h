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

//
// Spawn attributes
//

// proc_spawnattr_settype()
#define SPAWN_GROUP_MEMBER      0   /* Child process will be a member of the parent process group or the process group with the id given by the proc_spawnattr_setpgrp() */
#define SPAWN_GROUP_LEADER      1   /* Child process will be the leader of a new process group */
#define SPAWN_SESSION_LEADER    2   /* Child process will be the leader of a new session and the leader of a new process group */


#define _SPAWN_UMASK        1
#define _SPAWN_UID          2
#define _SPAWN_GID          4
#define _SPAWN_SUSPENDED    8

#define _SPAWNATTR_VERSION  1


typedef struct proc_spawnattr {
    size_t          version;
    int             type;
    pid_t           pgrp;
    fs_perms_t      umask;
    uid_t           uid;
    gid_t           gid;
    unsigned int    flags;
    int             quantum_boost;
    int             nice;
} proc_spawnattr_t;



//
// Spawn actions
//

#define _SPAWN_AT_SETCWD        1
#define _SPAWN_AT_SETROOTDIR    2
#define _SPAWN_AT_PASSFD        3
#define _SPAWN_AT_SHAREFD       4

#define _SPAWN_ACTIONS_VERSION  1

struct _proc_spawn_fd_map {
    int fd;
    int to_fd;
};

typedef struct _proc_spawn_action {
    int     type;
    union {
        char* _Nonnull              path;
        struct _proc_spawn_fd_map   fd_map;
    }       u;
} _proc_spawn_action_t;


typedef struct proc_spawn_actions {
    size_t                          version;
    size_t                          capacity;
    size_t                          count;
    _proc_spawn_action_t* _Nullable action;
} proc_spawn_actions_t;



//
// Spawn result
//

#define SPAWN_PHASE_CREATE  1
#define SPAWN_PHASE_ACTIONS 2
#define SPAWN_PHASE_EXEC    3

typedef struct proc_spawnres {
    pid_t   pid;
    int     err_phase;      // if called failed, phase in which the error happened
    int     err_info;       // if called failed and phase is ACTIONS, the index of the action that failed
} proc_spawnres_t;

#endif /* _KPI_SPAWN_H */
