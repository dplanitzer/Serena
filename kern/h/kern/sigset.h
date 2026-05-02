//
//  kern/sigset.h
//  libc
//
//  Created by Dietmar Planitzer on 9/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_SIGSET_H
#define _KERN_SIGSET_H 1

#include <stdbool.h>
#include <ext/try.h>
#include <kpi/signal.h>

extern const sigset_t SIGSET_IGNORE_ALL;
extern const sigset_t SIGSET_NON_ROUTABLE;
extern const sigset_t SIGSET_PRIV_SYS;


// Initializes the signal set 'set' to a set with the single signal 'signo'.
// Returns an error if 'signo' is not a valid signal number.
extern errno_t sigset_init(sigset_t* _Nonnull set, int signo);

// Removes all signals from the signal set 'set' and sets it to 0.
extern void sigset_clear(sigset_t* _Nonnull set);

// Adds all signals to the signal set 'set'.
extern void sigset_fill(sigset_t* _Nonnull set);

// Adds the signal 'signo' to the signal set 'set'. Returns an error if 'signo'
// is not a valid signal number.
extern errno_t sigset_add(sigset_t* _Nonnull set, int signo);

// Adds all signals from the set 'oth' to 'set'.
extern void sigset_addall(sigset_t* _Nonnull set, const sigset_t* _Nonnull oth);

// Removes the signal 'signo' from the signal set 'set'. Returns an error if
// 'signo' is not a valid signal number.
extern errno_t sigset_remove(sigset_t* _Nonnull set, int signo);

// Removes all signals listed in the signal set 'oth' from 'set'.
extern void sigset_removeall(sigset_t* _Nonnull set, const sigset_t* _Nonnull oth);

// Returns true if the signal set 'set' contains teh signal 'signo' and false
// otherwise.
extern bool sigset_contains(const sigset_t* _Nonnull set, int signo);

// Returns true if the signal set 'set' is empty and false otherwise.
extern bool sigset_isempty(const sigset_t* _Nonnull set);

#endif /* _KERN_SIGSET_H */
