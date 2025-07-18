//
//  sigset.c
//  libc
//
//  Created by Dietmar Planitzer on 7/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <limits.h>
#include <signal.h>


int sigemptyset(sigset_t* _Nonnull set)
{
    *set = 0;
    return 0;
}

int sigfillset(sigset_t* _Nonnull set)
{
    *set = UINT_MAX;
    return 0;
}

int sigaddset(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        errno = EINVAL;
        return -1;
    }

    *set |= _SIGBIT(signo);
    return 0;
}

int sigdelset(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        errno = EINVAL;
        return -1;
    }

    *set &= ~_SIGBIT(signo);
    return 0;
}

int sigismember(const sigset_t* _Nonnull set, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        errno = EINVAL;
        return -1;
    }

    if (*set & _SIGBIT(signo)) {
        return 1;
    }
    else {
        return 0;
    }
}
