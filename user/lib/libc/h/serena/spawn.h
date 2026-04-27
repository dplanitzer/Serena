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
#include <stdbool.h>
#include <kpi/spawn.h>

// Initializes a spawn attribute object such that a child process will be created
// with the following properties:
// - member of the parent process group
// - inherits the parent's uid and gid
// - inherits the parent's umask
// - inherits the parent's root directory
// - inherits the parent's current working directory
// - child process is automatically resumed
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

// Instructs the proc_spawn() call to set the umask of the newly spawned
// process to the provided umask rather than the parent's umask.
extern fs_perms_t proc_spawnattr_umask(const proc_spawnattr_t* _Nonnull attr);
extern int proc_spawnattr_setumask(proc_spawnattr_t* _Nonnull attr, fs_perms_t umask);

// The new process should use the provided user id rather than the parent process
// user id. Parent process must be the superuser (XXX for now).
extern uid_t proc_spawnattr_uid(const proc_spawnattr_t* _Nonnull attr);
extern int proc_spawnattr_setuid(proc_spawnattr_t* _Nonnull attr, uid_t uid);

// The new process should use the provided group id rather than the parent
// process group id. Parent process must be the superuser (XXX for now).
extern gid_t proc_spawnattr_gid(const proc_spawnattr_t* _Nonnull attr);
extern int proc_spawnattr_setgid(proc_spawnattr_t* _Nonnull attr, gid_t gid);

// The new process should start out in suspended state and the parent process
// will resume it by calling proc_resume() when it is ready to do so.
extern bool proc_spawnattr_suspended(const proc_spawnattr_t* _Nonnull attr);
extern void proc_spawnattr_setsuspended(proc_spawnattr_t* _Nonnull attr, bool flag);



extern int proc_spawn_actions_init(proc_spawn_actions_t* _Nonnull actions);
extern int proc_spawn_actions_destroy(proc_spawn_actions_t* _Nullable actions);

extern int proc_spawn_actions_setcwd(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path);
extern int proc_spawn_actions_setrootdir(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path);



extern int proc_spawn(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable envp[], const proc_spawnattr_t* _Nonnull attr, const proc_spawn_actions_t* _Nullable actions, proc_spawnres_t* _Nullable result);

#endif /* _SERENA_SPAWN_H */
