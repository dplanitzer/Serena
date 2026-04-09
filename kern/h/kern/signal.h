//
//  kern/signal.h
//  libc
//
//  Created by Dietmar Planitzer on 9/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_SIGNAL_H
#define _KERN_SIGNAL_H 1

#include <stdbool.h>
#include <ext/try.h>
#include <kpi/signal.h>

extern const sigset_t SIGSET_IGNORE_ALL;
extern const sigset_t SIGSET_NON_ROUTABLE;
extern const sigset_t SIGSET_PRIV_SYS;


extern void sigset_empty(sigset_t* _Nonnull set);
extern void sigset_all(sigset_t* _Nonnull set);
extern errno_t sigset_add(sigset_t* _Nonnull set, int signo);
extern errno_t sigset_remove(sigset_t* _Nonnull set, int signo);
extern bool sigset_contains(const sigset_t* _Nonnull set, int signo);

#endif /* _KERN_SIGNAL_H */
