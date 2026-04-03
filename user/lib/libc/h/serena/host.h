//
//  host.h
//  libc
//
//  Created by Dietmar Planitzer on 3/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_HOST_H
#define _SYS_HOST_H 1

#include <kpi/host.h>

__CPP_BEGIN

// Fills the buffer with an array of pid_t's of all currently existing processes.
// 'bufSize' is the size of the buffer in terms of the number of pid_t objects
// that it can hold. 'buf' will be terminated by a 0 pid_t. Thus 'bufSize' is
// expected to be equal to the number of pid_t's that should be returned plus 1.
// Returns 0 on success and if the buffer is big enough to hold the ids of
// all existing processes. Returns 1 if the buffer isn't big enough to hold the
// ids of all processes. However the buffer is still filled with as many ids as
// fit. Returns -1 on an error.
//
// Note that the list of returned processes includes processes in all possible
// states, including the zombie state.
extern int host_procs(pid_t* _Nonnull buf, size_t bufSize);

// Returns information about the local host. 'flavor' selects the kinds of
// information that should be returned.
extern int host_info(int flavor, host_info_ref _Nonnull info);

//XXX NOT YET
//extern int host_reboot();
//extern int host_shutdown();
//XXX

__CPP_END

#endif /* _SYS_HOST_H */
