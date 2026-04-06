//
//  serena/ioctl.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_IOCTL_H
#define _SERENA_IOCTL_H 1

#include <_cmndef.h>
#include <kpi/ioctl.h>

__CPP_BEGIN

// Invokes a I/O resource specific method on the descriptor 'fd'.
// @Concurrency: Safe
extern int ioctl(int fd, int cmd, ...);

__CPP_END

#endif /* _SERENA_IOCTL_H */
