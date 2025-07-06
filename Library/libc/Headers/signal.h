//
//  sys/signal.h
//  libc
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H 1

#include <_cmndef.h>
#include <_null.h>
#include <kpi/signal.h>
#include <sys/types.h>

__CPP_BEGIN

extern int sigemptyset(sigset_t* _Nonnull set);
extern int sigfillset(sigset_t* _Nonnull set);
extern int sigaddset(sigset_t* _Nonnull set, int signo);
extern int sigdelset(sigset_t* _Nonnull set, int signo);
extern int sigismember(const sigset_t* _Nonnull set, int signo);

#if 0

// Blocks the caller until one of the signals in 'set' becomes available.
// Returns the number of the highest priority signal and clears it from the set
// of pending signals. If 'mask' is not NULL then 'mask' is atomically
// installed as the signal mask as long as the wait is ongoing. Note that the
// signals in 'set' are always unblocked in addition to the signals left
// unblocked by the current signal mask. The current signal mask may leave
// signals unblocked that are not in 'set'. If one of those extra unblocked
// signals arrives then the wait is cancelled, the corresponding signal handler
// is invoked and EINTR is returned. Note however that this call will never
// invoke the signal handler for a signal in 'set'. This is true even if one is
// installed. Returns a positive value for the signal that was caught and that
// is part of 'set'. Returns EINTR if a signal is caught that isn't part of 'set'.
extern int sigwait(const sigset_t* _Nullable mask, const sigset_t* _Nonnull set);

// Similar to sigwait() but returns with ETIMEDOUT if the wait reached the timeout
// value.
extern int sigtimedwait(const sigset_t* _Nullable mask, const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp);

// Blocks the caller until a signal arrives and executes the corresponding
// signal handler to handle the signal. Temporarily installs 'mask' as the
// signal mask if 'mask' is not NULL.
extern int sigsuspend(const sigset_t* _Nullable mask);

// Same as sigsuspend() but with a timeout.
extern int sigtimedsuspend(const sigset_t* _Nullable mask, int flags, const struct timespec* _Nonnull wtp);

// Sends a signal to a process, process group, virtual processor or virtual
// processor group. 'scope' specifies to which scope the target identified by
// 'id' belongs. 'signo' is the number of the signal that should be sent.
extern int sigraise(int scope, id_t id, int signo);

#endif

__CPP_END

#endif /* _SYS_SIGNAL_H */
