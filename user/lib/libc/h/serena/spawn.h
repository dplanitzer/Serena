//
//  serena/spawn.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_SPAWN_H
#define _SERENA_SPAWN_H 1

#include <_cmndef.h>
#include <kpi/spawn.h>

__CPP_BEGIN

extern int os_spawn(const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull options, pid_t* _Nullable rpid);

__CPP_END

#endif /* _SERENA_SPAWN_H */
