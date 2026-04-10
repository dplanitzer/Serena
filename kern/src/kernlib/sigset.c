//
//  sigset.c
//  libc
//
//  Created by Dietmar Planitzer on 9/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <kern/signal.h>


const sigset_t SIGSET_IGNORE_ALL = 0;
const sigset_t SIGSET_NON_ROUTABLE = sig_bit(SIG_TERMINATE) | sig_bit(SIG_FORCE_SUSPEND) | sig_bit(SIG_RESUME) | sig_bit(SIG_VCPU_RELINQUISH) | sig_bit(SIG_VCPU_SUSPEND);
const sigset_t SIGSET_PRIV_SYS = sig_bit(SIG_VCPU_RELINQUISH) | sig_bit(SIG_VCPU_SUSPEND);


void sigset_empty(sigset_t* _Nonnull set)
{
    *set = 0;
}

void sigset_all(sigset_t* _Nonnull set)
{
    *set = UINT_MAX;
}

errno_t sigset_add(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    *set |= sig_bit(signo);
    return EOK;
}

errno_t sigset_remove(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    *set &= ~sig_bit(signo);
    return EOK;
}

bool sigset_contains(const sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        return false;
    }

    if (*set & sig_bit(signo)) {
        return true;
    }
    else {
        return false;
    }
}
