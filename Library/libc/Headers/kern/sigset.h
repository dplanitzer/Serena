//
//  kern/sigset.h
//  libc
//
//  Created by Dietmar Planitzer on 9/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_SIGSET_H
#define _KERN_SIGSET_H 1

#include <kern/errno.h>
#include <kern/types.h>
#include <kpi/signal.h>

extern void sigemptyset(sigset_t* _Nonnull set);
extern void sigfillset(sigset_t* _Nonnull set);
extern errno_t sigaddset(sigset_t* _Nonnull set, int signo);
extern errno_t sigdelset(sigset_t* _Nonnull set, int signo);
extern bool sigismember(const sigset_t* _Nonnull set, int signo);

#endif /* _KERN_SIGSET_H */
