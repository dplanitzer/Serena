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

// Number of signals that a signal set can hold
#define SIGSET_SIZE 32


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
// only signal for which vcpus define a default behavior is SIG_FORCE_QUIT: it causes
// the vcpu to relinquish itself involuntary. All other signals are freely
// available on the vcpu level. This means that if a vcpu A sends a signal to
// vcpu B (in the same process), or you register a vcpu and signal with a kernel
// API then this signal will be sent directly to the vcpu using the vcpu scope
// and thus no default behavior will be applied to the signal.

#define SIG_MIN  1
#define SIG_MAX  32

// Ordered from highest to lowest priority
#define SIG_FORCE_QUIT      1   // Forced process termination, non-routable
#define SIG_SYSTEM          2   // Notifies a vcpu that some important system state has changed and that it should enter kernel space and reevaluate its state now
#define SIG_FORCE_STOP      3   // Forced process stop, non-routable
#define SIG_STOP            4   // TTY, voluntary process stop, default: stop process
#define SIG_CONTINUE        5   // TTY, default: continue process, non-routable
#define SIG_CPU_LIMIT       6   // kernel, process exceeded CPU time limit, default: terminate
#define SIG_LOGOUT          7   // XXX logind, user logged out, default: terminate
#define SIG_QUIT            8   // TTY, process quit, default: terminate
#define SIG_CANCEL          9   // TTY, process interrupt/cancel operation, default: terminate
#define SIG_RESERVED1       10  // Reserved for the OS
#define SIG_CHILD           11  // kernel, child process changed state, default: ignore
#define SIG_WIN_CHANGE      12  // TTY, console window size changed, default: ignore
#define SIG_BKG_READ        13  // TTY, background process attempt to read from terminal input, default: stop/suspend process
#define SIG_BKG_WRITE       14  // TTY, background process attempt to write to terminal output, default: stop/suspend process
#define SIG_RESERVED2       15  // Reserved for the OS
#define SIG_RESERVED3       16  // Reserved for the OS
#define SIG_USER_1          17  // User defined signals, default: ignore
#define SIG_USER_2          18
#define SIG_USER_3          19
#define SIG_USER_4          20
#define SIG_USER_5          21
#define SIG_USER_6          22
#define SIG_USER_7          23
#define SIG_USER_8          24
#define SIG_USER_9          25
#define SIG_USER_10         26
#define SIG_USER_11         27
#define SIG_USER_12         28
#define SIG_USER_13         29
#define SIG_USER_14         30
#define SIG_USER_15         31
#define SIG_USER_16         32

#define SIG_USER        SIG_USER_1

#define SIG_USER_MIN    SIG_USER_1
#define SIG_USER_MAX    SIG_USER_16


#define sig_bit(__signo) (1 << ((__signo) - 1))


#define SIG_TARGET_VCPU             0   /* vcpu inside this process */
#define SIG_TARGET_VCPU_GROUP       1   /* vcpu group inside this process */
#define SIG_TARGET_PROC             2   /* process with pid */
#define SIG_TARGET_PROC_CHILDREN    3   /* all immediate children of process with pid */
#define SIG_TARGET_PROC_GROUP       4   /* all processes in group with process group id */
#define SIG_TARGET_SESSION          5   /* all processes in this session */

#define SIG_ROUTE_DEL   0
#define SIG_ROUTE_ADD   1

#endif /* _KPI_SIGNAL_H */
