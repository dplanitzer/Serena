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


#define SIGMIN  1
#define SIGMAX  32

// Ordered from highest to lowest priority
#define SIGKILL     1
#define SIGSYS0     2
#define SIGSYS1     3
#define SIGABRT     4   //XXX NYI   <- abort()
#define SIGSTOP     5   //XXX NYI   <- TTY, stop/suspend process
#define SIGCONT     6   //XXX NYI   <- TTY, continue/resume process
#define SIGXLIM     7   //XXX NYI   <- kernel, process exceeded CPU/memory/file limit
#define SIGHUP      8   //XXX NYI   <- logind, user logged out
#define SIGQUIT     9   //XXX NYI   <- TTY, process quit
#define SIGINT     10   //XXX NYI   <- TTY, process interrupt
#define SIGALRM    11   //XXX NYI   <- clock_alarm()
#define SIGCHILD   12   //          <- kernel, child process terminated
#define SIGWINCH   13   //XXX NYI   <- TTY, console window size changed
#define SIGTTIN    14   //XXX NYI   <- TTY, background process attempt to read from terminal input
#define SIGTTOUT   15   //XXX NYI   <- TTY, background process attempt to write to terminal output
#define SIGFORE    16   //XXX NYI   <- TTY, process is now the foreground process
#define SIGBACK    17   //XXX NYI   <- TTY, process is now the background process
#define SIGSYS2    18
#define SIGSYS3    19
#define SIGUSR0    20
#define SIGUSR1    21
#define SIGUSR2    22
#define SIGUSR3    23
#define SIGUSR4    24
#define SIGUSR5    25
#define SIGUSR6    26
#define SIGUSR7    27
#define SIGUSR8    28
#define SIGUSR9    29
#define SIGUSR10   30
#define SIGUSR11   31
#define SIGDISP    32

#define SIGUSR      SIGUSR0


#define _SIGBIT(__signo) (1 << ((__signo) - 1))

#define SIGSET_NONMASKABLES _SIGBIT(SIGKILL)
#define SIGSET_URGENTS      _SIGBIT(SIGKILL)


#define SIG_SCOPE_VCPU          0   /* vcpu inside this process */
#define SIG_SCOPE_VCPU_GROUP    1   /* vcpu group inside this process */
#define SIG_SCOPE_PROC          2   /* process with pid */
#define SIG_SCOPE_PROC_CHILDREN 3   /* all immediate children of process with pid */
#define SIG_SCOPE_PROC_GROUP    4   /* all processes in group with process group id */
#define SIG_SCOPE_SESSION       5   /* all processes in this session */

#define SIG_ROUTE_DISABLE       0
#define SIG_ROUTE_ENABLE        1

#endif /* _KPI_SIGNAL_H */
