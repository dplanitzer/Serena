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

__CPP_BEGIN

extern int waitpid(pid_t pid, pstatus_t* _Nullable result);

__CPP_END

#endif /* _SYS_WAIT_H */
