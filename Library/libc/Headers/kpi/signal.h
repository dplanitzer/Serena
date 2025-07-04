//
//  kpi/signal.h
//  libc
//
//  Created by Dietmar Planitzer on 6/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_VCPU_H
#define _KPI_VCPU_H 1

typedef  unsigned int sigset_t;


// Replace the whole signal mask
#define SIG_SETMASK 0

// Enable the signals specified in the provided mask
#define SIG_BLOCK  1

// Disable the signals specified in the provided mask
#define SIG_UNBLOCK 2


#define SIGMIN  1
#define SIGMAX  32

#define SIGKILL     1
#define SIGSYNCH    2

#define SIG_NONMASKABLE (1 << SIGKILL)

#endif /* _KPI_VCPU_H */
