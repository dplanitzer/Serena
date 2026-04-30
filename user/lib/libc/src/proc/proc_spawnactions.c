//
//  proc_spawnactions.c
//  libc
//
//  Created by Dietmar Planitzer on 4/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <ext/math.h>
#include <kpi/fd.h>
#include <serena/spawn.h>
#include <stdlib.h>
#include <string.h>

int proc_spawn_actions_init(proc_spawn_actions_t* _Nonnull actions)
{
    actions->version = _SPAWN_ACTIONS_VERSION;
    actions->action = NULL;
    actions->count = 0;
    actions->capacity = 0;
    
    return 0;
}

static void _proc_spawn_actions_free(_proc_spawn_action_t* _Nonnull act)
{
    switch (act->type) {
        case _SPAWN_AT_SETCWD:
        case _SPAWN_AT_SETROOTDIR:
            free(act->u.path);
            break;

        default:
            break;
    }
}

int proc_spawn_actions_destroy(proc_spawn_actions_t* _Nullable actions)
{
    if (actions) {
        for (size_t i = 0; i < actions->count; i++) {
            _proc_spawn_actions_free(&actions->action[i]);
        }

        free(actions->action);
        actions->action = NULL;
        actions->count = 0;
        actions->capacity = 0;
    }

    return 0;
}

static int _proc_spawn_actions_add(proc_spawn_actions_t* _Nonnull actions, const _proc_spawn_action_t* _Nonnull act, size_t act_count)
{
    if (actions->count == actions->capacity) {
        const size_t min_new_capacity = (actions->capacity > 0) ? actions->capacity * 2 : 8;
        const size_t new_capacity = __max(min_new_capacity, actions->capacity + act_count);
        _proc_spawn_action_t* new_act = realloc(actions->action, new_capacity * sizeof(struct _proc_spawn_action));

        if (new_act == NULL) {
            return -1;
        }

        actions->action = new_act;
        actions->capacity = new_capacity;
    }

    for (size_t i = 0; i < act_count; i++) {
        actions->action[actions->count + i] = act[i];
    }
    actions->count += act_count;
}

static int _proc_spawn_actions_addpathaction(proc_spawn_actions_t* _Nonnull actions, int type, const char* _Nonnull path)
{
    _proc_spawn_action_t act;

    if (*path == '\0') {
        errno = EINVAL;
        return -1;
    }

    act.type = type;
    act.u.path = strdup(path);
    if (act.u.path == NULL) {
        return -1;
    }

    const int r = _proc_spawn_actions_add(actions, &act, 1);
    if (r == -1) {
        free(act.u.path);
    }

    return r;
}

int proc_spawn_actions_setcwd(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path)
{
    return _proc_spawn_actions_addpathaction(actions, _SPAWN_AT_SETCWD, path);
}

int proc_spawn_actions_setrootdir(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path)
{
    return _proc_spawn_actions_addpathaction(actions, _SPAWN_AT_SETROOTDIR, path);
}

int proc_spawn_actions_pass_fd(proc_spawn_actions_t* _Nonnull actions, int fd, int to_fd)
{
    _proc_spawn_action_t act;

    act.type = _SPAWN_AT_PASSFD;
    act.u.fd_map.fd = fd;
    act.u.fd_map.to_fd = fd;

    return _proc_spawn_actions_add(actions, &act, 1);
}

int proc_spawn_actions_share_fd(proc_spawn_actions_t* _Nonnull actions, int fd, int to_fd)
{
    _proc_spawn_action_t act;

    act.type = _SPAWN_AT_SHAREFD;
    act.u.fd_map.fd = fd;
    act.u.fd_map.to_fd = fd;

    return _proc_spawn_actions_add(actions, &act, 1);
}

int proc_spawn_actions_share_stdio(proc_spawn_actions_t* _Nonnull actions)
{
    _proc_spawn_action_t act[3];

    act[0].type = _SPAWN_AT_SHAREFD;
    act[0].u.fd_map.fd = FD_STDIN;
    act[0].u.fd_map.to_fd = FD_STDIN;

    act[1].type = _SPAWN_AT_SHAREFD;
    act[1].u.fd_map.fd = FD_STDOUT;
    act[1].u.fd_map.to_fd = FD_STDOUT;

    act[2].type = _SPAWN_AT_SHAREFD;
    act[2].u.fd_map.fd = FD_STDERR;
    act[2].u.fd_map.to_fd = FD_STDERR;

    return _proc_spawn_actions_add(actions, &act[0], 3);
}
