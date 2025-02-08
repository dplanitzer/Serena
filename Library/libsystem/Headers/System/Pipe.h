//
//  Pipe.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PIPE_H
#define _SYS_PIPE_H 1

#include <System/_cmndef.h>
#include <System/_syslimits.h>
#include <System/Error.h>
#include <System/IOChannel.h>
#include <System/Types.h>

__CPP_BEGIN

#if !defined(__KERNEL__)

// Creates an anonymous pipe and returns a read and write I/O channel to the pipe.
// Data which is written to the pipe using the write I/O channel can be read using
// the read I/O channel. The data is made available in first-in-first-out order.
// Note that both I/O channels must be closed to free all pipe resources.
extern errno_t Pipe_Create(int* _Nonnull rioc, int* _Nonnull wioc);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_PIPE_H */
