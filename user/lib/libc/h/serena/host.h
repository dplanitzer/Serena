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
#include <serena/types.h>

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
// The optional 'matchers' array specifies criteria that a process must match in
// order to be included in the result. The array is a list of PROC_MATCHING
// macros which must be terminated by a PROC_MATCHING_END() macro. Note that
// currently at most one matching criteria can be specified. If 'matchers' is
// NULL then all existing processes are returned.
//
// Note that the list of returned processes includes processes in all possible
// states, including the zombie state.
extern int host_procs(const proc_matcher_t* _Nullable matchers, pid_t* _Nonnull buf, size_t bufSize);

__CPP_END

#endif /* _SYS_HOST_H */
