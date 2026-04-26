//
//  spawn.h
//  libc
//
//  Created by Dietmar Planitzer on 4/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_SPAWN_H
#define _SERENA_SPAWN_H 1

#include <_cmndef.h>
#include <kpi/spawn.h>

// Initializes a spawn attribute object such that a child process will be created
// with the following properties:
// - member of the parent process group
// - inherits the parent's uid and gid
// - inherits the parent's umask
// - inherits the parent's root directory
// - inherits the parent's current working directory
extern int proc_spawnattr_init(proc_spawnattr_t* _Nonnull attr);
extern int proc_spawnattr_destroy(proc_spawnattr_t* _Nullable attr);

// Sets the type of spawn operation that should be executed: whether the child
// process should become the member of an existing process group, the leader of
// a new process group or the leader of a new user session. Note that this
// function resets the process group id to 0, which means that the child process
// will join the parent's process group if PROC_SPAWN_GROUP_MEMBER is set. Use
// the proc_spawnattr_setpgrp() API to set the desired process group id. 
extern int proc_spawnattr_type(const proc_spawnattr_t* _Nonnull attr);
extern int proc_spawnattr_settype(proc_spawnattr_t* _Nonnull attr, int type);

// Sets the id of the process group that the child process should join if the
// spawn type was set to PROC_SPAWN_GROUP_MEMBER.
extern pid_t proc_spawnattr_pgrp(const proc_spawnattr_t* _Nonnull attr);
extern int proc_spawnattr_setpgrp(proc_spawnattr_t * _Nonnull attr, pid_t pgrp);



extern int proc_spawn_actions_init(proc_spawn_actions_t* _Nonnull actions);
extern int proc_spawn_actions_destroy(proc_spawn_actions_t* _Nullable actions);

extern int proc_spawn_actions_setcwd(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path);
extern int proc_spawn_actions_setrootdir(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path);



extern int proc_spawn(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable envp[], const proc_spawnattr_t* _Nonnull attr, const proc_spawn_actions_t* _Nullable actions, pid_t* _Nullable rpid);

#endif /* _SERENA_SPAWN_H */
