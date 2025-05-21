//
//  sys/wait.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H 1

#include <_cmndef.h>
#include <kpi/wait.h>
#include <sys/types.h>

__CPP_BEGIN

#define WIFEXITED(stat_val) \
(((stat_val) & _WREASONMASK) == _WNORMTERM)

#define WEXITSTATUS(stat_val) \
((stat_val) & _WSTATUSMASK)

#define WIFSIGNALED(stat_val) \
(((stat_val) & _WREASONMASK) == _WSIGNALED)

#define WTERMSIG(stat_val) \
((stat_val) & _WSIGNUMMASK)


extern pid_t waitpid(pid_t pid, int* _Nullable pstat, int options);

__CPP_END

#endif /* _SYS_WAIT_H */
