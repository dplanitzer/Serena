//
//  sys/proc.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PROC_H
#define _SYS_PROC_H 1

#include <_cmndef.h>
#include <kpi/proc.h>

__CPP_BEGIN

extern pargs_t* _Nonnull getpargs(void);

__CPP_END

#endif /* _SYS_PROC_H */
