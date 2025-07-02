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
#include <sys/types.h>

__CPP_BEGIN

extern int sigemptyset(sigset_t* _Nonnull set);
extern int sigfillset(sigset_t* _Nonnull set);
extern int sigaddset(sigset_t* _Nonnull set, int signo);
extern int sigdelset(sigset_t* _Nonnull set, int signo);
extern int sigismember(const sigset_t* _Nonnull set, int signo);


// Atomically and temporarily replaces the signal mask of the calling vcpu with
// 'mask' if not NULL and then checks whether at least one signal enabled by the
// mask is pending. This signal and all other pending and enabled signals are
// returned in 'sigs' and then cleared from the pending signal set. If 'mask'
// is NULL then the current signal mask of the calling vcpu is used. The
// original signal mask of the vcpu is reestablished after the wait completes
// and before this call returns.
extern int sig_wait(const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs);

// Like sig_wait() but limits the waiting time to the timeout 'wtp'. 'wtp' is by
// default interpreted as a duration that will be added to the current time. Pass
// TIMER_ABSTIME if 'wtp' should be interpreted as an absolute point in time
// instead.
extern int sig_timedwait(const sigset_t* _Nullable mask, sigset_t* _Nonnull sigs, int flags, const struct timespec* _Nonnull wtp);

// Sends the signal 'signo' to the vcpu 'vcpu'.
extern int sig_raise(vcpuid_t vcpu, int signo);


extern sigset_t sig_getmask(void);

extern int sig_setmask(int op, sigset_t mask, sigset_t* _Nullable oldmask);

__CPP_END

#endif /* _SYS_SIGNAL_H */
