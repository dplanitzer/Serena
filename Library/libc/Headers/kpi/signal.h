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


// Replace the whole signal mask
#define SIG_SETMASK 0

// Enable the signals specified in the provided mask
#define SIG_BLOCK  1

// Disable the signals specified in the provided mask
#define SIG_UNBLOCK 2


#define SIGMIN  1
#define SIGMAX  32

#define SIGKILL     1
#define SIGDISPATCH 2


#define SIGSET_NONMASKABLES (1 << SIGKILL)
#define SIGSET_URGENT       (1 << SIGKILL)

#define _SIGBIT(__signo) (1 << ((__signo) - 1))


#define SIG_SCOPE_VCPU          0
#define SIG_SCOPE_VCPU_GROUP    1


typedef struct siginfo {
    int     signo;      // Signal number
//    pid_t   pid;        // Originating process
} siginfo_t;

#endif /* _KPI_SIGNAL_H */
