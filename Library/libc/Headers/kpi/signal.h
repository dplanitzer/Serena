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
#define SIGSUSPEND  2
#define SIGCHILD    3
#define SIGDISPATCH 32


#define _SIGBIT(__signo) (1 << ((__signo) - 1))

#define SIGSET_NONMASKABLES (_SIGBIT(SIGKILL) | _SIGBIT(SIGSUSPEND))
#define SIGSET_URGENTS      (_SIGBIT(SIGKILL) | _SIGBIT(SIGSUSPEND))


#define SIG_SCOPE_VCPU          0   /* vcpu inside this process */
#define SIG_SCOPE_VCPU_GROUP    1   /* vcpu group inside this process */
#define SIG_SCOPE_PROC          2   /* process with pid */
#define SIG_SCOPE_PROC_CHILDREN 3   /* all immediate children of process with pid */
#define SIG_SCOPE_PROC_GROUP    4   /* all processes in group with process group id */
#define SIG_SCOPE_SESSION       5   /* all processes in this session */

#define SIG_ROUTE_DISABLE       0
#define SIG_ROUTE_ENABLE        1

#endif /* _KPI_SIGNAL_H */
