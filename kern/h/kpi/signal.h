//
//  kpi/signal.h
//  kpi
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SIGNAL_H
#define _KPI_SIGNAL_H 1

typedef unsigned int sigset_t;
typedef volatile int sig_atomic_t;


// The system supports 32 different signals. You send a signal to a particular
// 'signal scope'. The signal scope determines who will receive the signal and
// whether there is default behavior associated with the signal.
//
// Signal scopes are organized in hierarchy like this:
// * user session: all process groups in the session
// * process group: all processes in the group
// * process
// * vcpu group: all vcpus in the group
// * vcpu
//
// User session and process group scopes defer the default behavior for signals
// to the process scope. The process scope default behaviors are documented in
// the signal list below.
//
// Vcpu groups defer the default behavior for signals to the vcpu scope. The
// only signal for which vcpus define a default behavior is SIG_TERMINATE: it causes
// the vcpu to relinquish itself involuntary. All other signals are freely
// available on the vcpu level. This means that if a vcpu A sends a signal to
// vcpu B (in the same process), or you register a vcpu and signal with a kernel
// API then this signal will be sent directly to the vcpu using the vcpu scope
// and thus no default behavior will be applied to the signal.

#define SIG_MIN  1
#define SIG_MAX  32

// Ordered from highest to lowest priority
#define SIG_TERMINATE       1   // Forced process termination, non-routable
#define SIG_VCPU_RELINQUISH 2   // Privileged signal
#define SIG_VCPU_SUSPEND    3   // Privileged signal
#define SIG_FORCE_SUSPEND   4   // Forced process suspend, non-routable
#define SIG_SUSPEND         5   // TTY, stop/suspend process, default: stop/suspend process
#define SIG_RESUME          6   // TTY, default: continue/resume process, non-routable
#define SIG_CPU_LIMIT       7   // kernel, process exceeded CPU time limit, default: terminate
#define SIG_LOGOUT          8   // XXX logind, user logged out, default: terminate
#define SIG_QUIT            9   // TTY, process quit, default: terminate
#define SIG_INTERRUPT       10  // TTY, process interrupt, default: ignore
#define SIG_TIMEOUT         11  // XXX clock_alarm(), default: ignore
#define SIG_CHILD           12  // kernel, child process changed state, default: ignore
#define SIG_WIN_CHANGE      13  // TTY, console window size changed, default: ignore
#define SIG_BKG_READ        14  // TTY, background process attempt to read from terminal input, default: stop/suspend process
#define SIG_BKG_WRITE       15  // TTY, background process attempt to write to terminal output, default: stop/suspend process
#define SIG_USER_1          16  // User defined signals, default: ignore
#define SIG_USER_2          17
#define SIG_USER_3          18
#define SIG_USER_4          19
#define SIG_USER_5          20
#define SIG_USER_6          21
#define SIG_USER_7          22
#define SIG_USER_8          23
#define SIG_USER_9          24
#define SIG_USER_10         25
#define SIG_USER_11         26
#define SIG_USER_12         27
#define SIG_USER_13         28
#define SIG_USER_14         29
#define SIG_USER_15         30
#define SIG_USER_16         31
#define SIG_USER_17         32

#define SIG_USER        SIG_USER_1

#define SIG_USER_MIN    SIG_USER_1
#define SIG_USER_MAX    SIG_USER_17


#define sig_bit(__signo) (1 << ((__signo) - 1))

#define SIGSET_NONMASKABLES (sig_bit(SIG_TERMINATE) | sig_bit(SIG_VCPU_RELINQUISH))
#define SIGSET_URGENTS      (sig_bit(SIG_TERMINATE) | sig_bit(SIG_VCPU_RELINQUISH) | sig_bit(SIG_VCPU_SUSPEND))


#define SIG_SCOPE_VCPU          0   /* vcpu inside this process */
#define SIG_SCOPE_VCPU_GROUP    1   /* vcpu group inside this process */
#define SIG_SCOPE_PROC          2   /* process with pid */
#define SIG_SCOPE_PROC_CHILDREN 3   /* all immediate children of process with pid */
#define SIG_SCOPE_PROC_GROUP    4   /* all processes in group with process group id */
#define SIG_SCOPE_SESSION       5   /* all processes in this session */

#define SIG_ROUTE_DEL   0
#define SIG_ROUTE_ADD   1

#endif /* _KPI_SIGNAL_H */
