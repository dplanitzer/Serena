//
//  _syscall.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYSCALL_H
#define _SYSCALL_H 1

#include <_cmndef.h>
#include <_syscalldef.h>
#include <errno.h>

__CPP_BEGIN

extern int __syscall(int scno, ...);

__CPP_END

#endif /* _SYSCALL_H */
