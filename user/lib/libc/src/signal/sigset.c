//
//  sigset.c
//  libc
//
//  Created by Dietmar Planitzer on 7/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <limits.h>
#include <serena/signal.h>


int sigset_empty(sigset_t* _Nonnull set)
{
    *set = 0;
    return 0;
}

int sigset_all(sigset_t* _Nonnull set)
{
    *set = UINT_MAX;
    return 0;
}

int sigset_add(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        errno = EINVAL;
        return -1;
    }

    *set |= sig_bit(signo);
    return 0;
}

int sigset_remove(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        errno = EINVAL;
        return -1;
    }

    *set &= ~sig_bit(signo);
    return 0;
}

int sigset_contains(const sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        errno = EINVAL;
        return -1;
    }

    if (*set & sig_bit(signo)) {
        return 1;
    }
    else {
        return 0;
    }
}
