//
//  kpi/signal.h
//  libc
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
// only signal for which vcpus define a default behavior is SIGKILL: it causes
// the vcpu to relinquish itself involuntary. All other signals are freely
// available on the vcpu level. This means that if a vcpu A sends a signal to
// vcpu B (in the same process), or you register a vcpu and signal with a kernel
// API then this signal will be sent directly to the vcpu using the vcpu scope
// and thus no default behavior will be applied to the signal.

#define SIGMIN  1
#define SIGMAX  32

// Ordered from highest to lowest priority
#define SIGKILL     1
#define SIGSYS1     2
#define SIGSYS2     3
#define SIGABRT     4   //XXX NYI   <- abort()
#define SIGSTOP     5   //XXX NYI   <- TTY, stop/suspend process
#define SIGCONT     6   //XXX NYI   <- TTY, continue/resume process
#define SIGXCPU     7   //XXX NYI   <- kernel, process exceeded CPU time limit
#define SIGHUP      8   //XXX NYI   <- logind, user logged out
#define SIGQUIT     9   //XXX NYI   <- TTY, process quit
#define SIGINT      10  //XXX NYI   <- TTY, process interrupt
#define SIGALRM     11  //XXX NYI   <- clock_alarm()
#define SIGCHILD    12  //          <- kernel, child process terminated
#define SIGWINCH    13  //XXX NYI   <- TTY, console window size changed
#define SIGTTIN     14  //XXX NYI   <- TTY, background process attempt to read from terminal input
#define SIGTTOUT    15  //XXX NYI   <- TTY, background process attempt to write to terminal output
#define SIGFRGD     16  //XXX NYI   <- TTY, process is now the foreground process
#define SIGBKGD     17  //XXX NYI   <- TTY, process is now the background process
#define SIGUSR1     18
#define SIGUSR2     19
#define SIGUSR3     20
#define SIGUSR4     21
#define SIGUSR5     22
#define SIGUSR6     23
#define SIGUSR7     24
#define SIGUSR8     25
#define SIGUSR9     26
#define SIGUSR10    27
#define SIGUSR11    28
#define SIGUSR12    29
#define SIGUSR13    30
#define SIGUSR14    31
#define SIGDISP     32

#define SIGUSR      SIGUSR1


#define _SIGBIT(__signo) (1 << ((__signo) - 1))

#define SIGSET_NONMASKABLES _SIGBIT(SIGKILL)
#define SIGSET_URGENTS      (_SIGBIT(SIGKILL) | _SIGBIT(SIGSYS1))


#define SIG_SCOPE_VCPU          0   /* vcpu inside this process */
#define SIG_SCOPE_VCPU_GROUP    1   /* vcpu group inside this process */
#define SIG_SCOPE_PROC          2   /* process with pid */
#define SIG_SCOPE_PROC_CHILDREN 3   /* all immediate children of process with pid */
#define SIG_SCOPE_PROC_GROUP    4   /* all processes in group with process group id */
#define SIG_SCOPE_SESSION       5   /* all processes in this session */

#define SIG_ROUTE_DISABLE       0
#define SIG_ROUTE_ENABLE        1

#endif /* _KPI_SIGNAL_H */
