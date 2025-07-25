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


// Updates the signal routing table that determines which vcpu should receive a
// signal that is sent to the process (from the outside or the inside). Signals
// which target the process are by default not routed at all and are subject to
// default processing. Use this function to enable a vcpu to receive a process
// targeted signal or to turn the reception of signals off for a particular vcpu.
// 'scope' may be SIG_SCOPE_VCPU or SIG_SCOPE_VCPU_GROUP. Routing is enabled if
// 'op' is SIG_ROUTE_ENABLE and disabled if 'op' is SIG_ROUTE_DISABLE. Note that
// the enabled state is reference counted. This means that if you enable a route
// to a vcpu X twice, that you must disable it twice to truly disable the routing
// of signals to this vcpu.
extern int sigroute(int scope, id_t id, int op);

// Blocks the caller until one of the signals in 'set' is delivered to the vcpu.
// Returns the highest priority pending signal in 'signo' and clears it from the
// list of pending signals.
extern int sigwait(const sigset_t* _Nonnull set, int* _Nonnull signo);

// Similar to sigwait() but returns with ETIMEDOUT if the wait reached the timeout
// value.
extern int sigtimedwait(const sigset_t* _Nonnull set, int flags, const struct timespec* _Nonnull wtp, int* _Nonnull signo);

// Returns the set of signals that are pending. This function does not consume
// pending signals and it does not trigger signal handlers.
extern int sigpending(sigset_t* _Nonnull set);

// Sends a signal to a process, process group, virtual processor or virtual
// processor group. 'scope' specifies to which scope the target identified by
// 'id' belongs. 'signo' is the number of the signal that should be sent. If
// 'scope' is SIG_SCOPE_PROC and 'id' is 0 then the calling process is targeted.
// If 'scope' is SIG_SCOPE_VCPU and 'id' is 0 then the calling vcpu is targeted.
// If 'scope' is SIG_SCOPE_PROC_CHILDREN and 'id' is 0 then all children of the
// calling process are targeted. If 'scope' is SIG_SCOPE_PROC_GROUP and 'id' is
// 0 then all members of the process group are targeted. If 'scope' is
// SIG_SCOPE_SESSION and 'id' is 0 then the session to which the calling process
// belongs is targeted. 
extern int sigsend(int scope, id_t id, int signo);

__CPP_END

#endif /* _SYS_SIGNAL_H */
