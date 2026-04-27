//
//  proc_spawnactions.c
//  libc
//
//  Created by Dietmar Planitzer on 4/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <serena/spawn.h>
#include <stdlib.h>
#include <string.h>

int proc_spawn_actions_init(proc_spawn_actions_t* _Nonnull actions)
{
    actions->version = sizeof(struct proc_spawn_actions);
    actions->action = NULL;
    actions->count = 0;
    actions->capacity = 0;
    
    return 0;
}

static void _proc_spawn_actions_free(_proc_spawn_action_t* _Nonnull act)
{
    switch (act->type) {
        case _PROC_SPACT_SETCWD:
        case _PROC_SPACT_SETROOTDIR:
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

static int _proc_spawn_actions_add(proc_spawn_actions_t* _Nonnull actions, _proc_spawn_action_t* _Nonnull act)
{
    if (actions->count == actions->capacity) {
        const size_t new_capacity = (actions->capacity > 0) ? actions->capacity * 2 : 4;
        _proc_spawn_action_t* new_act = realloc(actions->action, new_capacity * sizeof(struct _proc_spawn_action));

        if (new_act == NULL) {
            return -1;
        }

        actions->action = new_act;
        actions->capacity = new_capacity;
    }

    actions->action[actions->count] = *act;
    actions->count++;
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

    const int r = _proc_spawn_actions_add(actions, &act);
    if (r == -1) {
        free(act.u.path);
    }

    return r;
}

int proc_spawn_actions_setcwd(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path)
{
    return _proc_spawn_actions_addpathaction(actions, _PROC_SPACT_SETCWD, path);
}

int proc_spawn_actions_setrootdir(proc_spawn_actions_t* _Nonnull actions, const char* _Nonnull path)
{
    return _proc_spawn_actions_addpathaction(actions, _PROC_SPACT_SETROOTDIR, path);
}
