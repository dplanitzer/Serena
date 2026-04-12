//
//  pipe.h
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_PIPE_H
#define _SERENA_PIPE_H 1

#include <kpi/pipe.h>
#include <serena/fd.h>

__CPP_BEGIN

// Creates an anonymous pipe and returns a read and write I/O channel to the pipe.
// Data which is written to the pipe using the write I/O channel can be read using
// the read I/O channel. The data is made available in first-in-first-out order.
// Note that both I/O channels must be closed to free all pipe resources.
// The write channel is returned in fds[1] and the read channel in fds[0].
extern int pipe_create(int fds[2]);

__CPP_END

#endif /* _SERENA_PIPE_H */
