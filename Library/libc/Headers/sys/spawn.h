//
//  sys/spawn.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SPAWN_H
#define _SYS_SPAWN_H 1

#include <kern/_cmndef.h>
#include <sys/dispatch.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <kpi/spawn.h>

__CPP_BEGIN

extern int os_spawn(const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull options, pid_t* _Nullable rpid);

__CPP_END

#endif /* _SYS_SPAWN_H */
