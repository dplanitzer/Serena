//
//  __proc_init.c
//  libc
//
//  Created by Dietmar Planitzer on 4/27/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <__stdlib.h>
#include <kpi/fd.h>
#include <kpi/spawn.h>

static proc_spawn_actions_t    __gDfltSpawnActions;
static _proc_spawn_action_t    __gStdioActions[3];


void __proc_init(void)
{
    __gDfltSpawnActions.version = _SPAWN_ACTIONS_VERSION;
    __gDfltSpawnActions.action = &__gStdioActions[0];
    __gDfltSpawnActions.capacity = 3;
    __gDfltSpawnActions.count = 3;

    __gStdioActions[0].type = _SPAWN_AT_SHAREFD;
    __gStdioActions[0].u.fd_map.fd = FD_STDIN;
    __gStdioActions[0].u.fd_map.to_fd = FD_STDIN;

    __gStdioActions[1].type = _SPAWN_AT_SHAREFD;
    __gStdioActions[1].u.fd_map.fd = FD_STDOUT;
    __gStdioActions[1].u.fd_map.to_fd = FD_STDOUT;

    __gStdioActions[2].type = _SPAWN_AT_SHAREFD;
    __gStdioActions[2].u.fd_map.fd = FD_STDERR;
    __gStdioActions[2].u.fd_map.to_fd = FD_STDERR;
}

const proc_spawn_actions_t* _Nonnull __proc_default_spawn_actions(void)
{
    return &__gDfltSpawnActions;
}
