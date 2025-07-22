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

#define WIFEXITED(__code) \
(((__code) & _WREASONMASK) == _WNORMTERM)

#define WEXITSTATUS(__code) \
((__code) & _WSTATUSMASK)


#define WIFSIGNALED(__code) \
(((__code) & _WREASONMASK) == _WSIGNALED)

#define WTERMSIG(__code) \
((__code) & _WSTATUSMASK)


extern pid_t wait(int* _Nullable pstat);
extern pid_t waitpid(pid_t pid, int* _Nullable pstat, int options);

__CPP_END

#endif /* _SYS_WAIT_H */
