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
#define SIGKILL     1   // Forced process termination, non-routable
#define SIGVPRQ     2   // Privileged signal
#define SIGVPDS     3   // Privileged signal
#define SIGABRT     4   // abort(), default: terminate
#define SIGSTOP     5   // Forced process suspend, non-routable
#define SIGTSTP     6   // TTY, stop/suspend process, default: stop/suspend process
#define SIGCONT     7   // TTY, default: continue/resume process, non-routable
#define SIGXCPU     8   // kernel, process exceeded CPU time limit, default: terminate
#define SIGHUP      9   // XXX logind, user logged out, default: terminate
#define SIGQUIT     10   // TTY, process quit, default: terminate
#define SIGINT      11  // TTY, process interrupt, default: ignore
#define SIGALRM     12  // XXX clock_alarm(), default: ignore
#define SIGCHILD    13  // kernel, child process terminated, default: ignore
#define SIGWINCH    14  // TTY, console window size changed, default: ignore
#define SIGTTIN     15  // TTY, background process attempt to read from terminal input, default: stop/suspend process
#define SIGTTOUT    16  // TTY, background process attempt to write to terminal output, default: stop/suspend process
#define SIGUSR1     17  // User defined signals, default: ignore
#define SIGUSR2     18
#define SIGUSR3     19
#define SIGUSR4     20
#define SIGUSR5     21
#define SIGUSR6     22
#define SIGUSR7     23
#define SIGUSR8     24
#define SIGUSR9     25
#define SIGUSR10    26
#define SIGUSR11    27
#define SIGUSR12    28
#define SIGUSR13    29
#define SIGUSR14    30
#define SIGUSR15    31
#define SIGDISP     32  // libdispatch, default: ignore

#define SIGUSR      SIGUSR1

#define SIGUSRMIN   SIGUSR1
#define SIGUSRMAX   SIGUSR15


#define _SIGBIT(__signo) (1 << ((__signo) - 1))

#define SIGSET_NONMASKABLES (_SIGBIT(SIGKILL) | _SIGBIT(SIGVPRQ))
#define SIGSET_URGENTS      (_SIGBIT(SIGKILL) | _SIGBIT(SIGVPRQ) | _SIGBIT(SIGVPDS))


#define SIG_SCOPE_VCPU          0   /* vcpu inside this process */
#define SIG_SCOPE_VCPU_GROUP    1   /* vcpu group inside this process */
#define SIG_SCOPE_PROC          2   /* process with pid */
#define SIG_SCOPE_PROC_CHILDREN 3   /* all immediate children of process with pid */
#define SIG_SCOPE_PROC_GROUP    4   /* all processes in group with process group id */
#define SIG_SCOPE_SESSION       5   /* all processes in this session */

#define SIG_ROUTE_DEL   0
#define SIG_ROUTE_ADD   1

#endif /* _KPI_SIGNAL_H */
