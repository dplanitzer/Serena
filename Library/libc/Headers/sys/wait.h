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
#include <sys/timespec.h>

__CPP_BEGIN

extern int proc_join(int scope, pid_t id, struct proc_status* _Nonnull ps);
extern int proc_timedjoin(int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps);

__CPP_END

#endif /* _SYS_WAIT_H */
