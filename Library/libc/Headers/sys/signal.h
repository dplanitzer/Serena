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
#include <kpi/_time.h>

__CPP_BEGIN

extern int sigemptyset(sigset_t* _Nonnull set);
extern int sigfillset(sigset_t* _Nonnull set);
extern int sigaddset(sigset_t* _Nonnull set, int signo);
extern int sigdelset(sigset_t* _Nonnull set, int signo);
extern int sigismember(const sigset_t* _Nonnull set, int signo);


// Checks whether signals are pending and blocks the caller until at least one
// signal arrives. Which signals this function waits for is controlled by the
// signal mask of the calling thread of execution. Returns all pending signals
// in 'sigs' and resets the internal pending signal state.
extern int sig_wait(sigset_t* _Nullable sigs);

// Like sig_wait() but limits the waiting time to the timeout 'wtp'. 'wtp' is by
// default interpreted as a duration that will be added to the current time. Pass
// TIMER_ABSTIME if 'wtp' should be interpreted as an absolute point in time
// instead.
extern int sig_timedwait(int flags, const struct timespec* _Nonnull wtp, sigset_t* _Nullable sigs);

// Sends the signal 'signo' to the vcpu 'vcpu'.
extern int sig_raise(int vcpu, int signo);


extern sigset_t sig_getmask(void);

extern int sig_setmask(int op, sigset_t mask, sigset_t* _Nullable oldmask);

__CPP_END

#endif /* _SYS_SIGNAL_H */
