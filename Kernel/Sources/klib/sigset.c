//
//  sigset.c
//  libc
//
//  Created by Dietmar Planitzer on 9/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kern/signal.h>
#include <kern/limits.h>


const sigset_t SIGSET_IGNORE_ALL = 0;
const sigset_t SIGSET_NON_ROUTABLE = _SIGBIT(SIGKILL) | _SIGBIT(SIGSTOP) | _SIGBIT(SIGCONT) | _SIGBIT(SIGVPRQ) | _SIGBIT(SIGVPDS) | _SIGBIT(SIGDISP);


void sigemptyset(sigset_t* _Nonnull set)
{
    *set = 0;
}

void sigfillset(sigset_t* _Nonnull set)
{
    *set = UINT_MAX;
}

errno_t sigaddset(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }

    *set |= _SIGBIT(signo);
    return EOK;
}

errno_t sigdelset(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        return EINVAL;
    }

    *set &= ~_SIGBIT(signo);
    return EOK;
}

bool sigismember(const sigset_t* _Nonnull set, int signo)
{
    if (signo < SIGMIN || signo > SIGMAX) {
        return false;
    }

    if (*set & _SIGBIT(signo)) {
        return true;
    }
    else {
        return false;
    }
}
