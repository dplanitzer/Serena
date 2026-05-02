//
//  wait.h
//  libc
//
//  Created by Dietmar Planitzer on 5/02/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_WAIT_H
#define _SERENA_WAIT_H 1

#include <kpi/wait.h>

__CPP_BEGIN

// Waits until the specified child, any child or member of a child process group
// changes its state to 'wstate' or any new state. The function remains blocked
// until then if 'flags' does not have WAIT_NONBLOCKING set. If it has then this
// function returns immediately with EOK and the state change information or it
// returns EAGAIN if no state change has occurred. ECHILD is returned if the
// specified child does not exist.
extern int proc_waitstate(int wstate, int match, pid_t id, int flags, proc_waitres_t* _Nonnull res);

__CPP_END

#endif /* _SERENA_WAIT_H */
