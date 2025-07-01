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

// Waits on the wait queue if no signals are pending. Wakes up as soon as a signal
// is sent to the wait queue. Returns the pending signals in 'psigs'. Note that
// this calls consumes all pending signals.
extern int sig_wait(unsigned int* _Nullable psigs);

// Waits on the wait queue until another vcpu calls wakeup() on the queue or
// until the timeout 'wtp' is reached. Whatever comes first. 'wtp' is by default
// interpreted as a duration that will be added to the current time. Pass
// TIMER_ABSTIME if 'wtp' should be interpreted as an absolute point in time
// instead.
extern int sig_timedwait(int flags, const struct timespec* _Nonnull wtp, unsigned int* _Nullable psigs);

// Sends signals to all vcpus waiting on the wait queue. 'sigs' is the set of
// signals that should be sent. These signals are merged into the pending signal
// set.
extern int sig_raise(int vcpu, int signo);


extern unsigned int sig_getmask(void);

extern int sig_setmask(int op, unsigned int mask, unsigned int* _Nullable oldmask);

__CPP_END

#endif /* _SYS_SIGNAL_H */
