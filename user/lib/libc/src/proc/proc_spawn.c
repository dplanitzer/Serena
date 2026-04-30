//
//  proc_spawn.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stddef.h>
#include <serena/fd.h>
#include <serena/spawn.h>
#include <kpi/syscall.h>

extern const proc_spawn_actions_t* _Nonnull __proc_default_spawn_actions(void);


int proc_spawn(const char* _Nonnull path, const char* _Nullable argv[], const char* _Nullable envp[], const proc_spawnattr_t* _Nonnull attr, const proc_spawn_actions_t* _Nullable actions, proc_spawnres_t* _Nullable result)
{
    if (actions == NULL) {
        actions = __proc_default_spawn_actions();
    }

    const int r = (int)_syscall(SC_proc_spawn, path, argv, envp, attr, actions, result);

    if (r == 0 && actions) {
        // close all pass_fd actions on spawn success
        for (size_t i = 0; i < actions->count; i++) {
            const _proc_spawn_action_t* act = &actions->action[i];

            if (act->type == _SPAWN_AT_PASSFD) {
                fd_close(act->u.fd_map.fd);
            }
        }
    }

    return r;
}
