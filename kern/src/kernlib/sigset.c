//
//  sigset.c
//  libc
//
//  Created by Dietmar Planitzer on 9/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <kern/sigset.h>


const sigset_t SIGSET_IGNORE_ALL = 0;
const sigset_t SIGSET_NON_ROUTABLE = sig_bit(SIG_TERMINATE) | sig_bit(SIG_FORCE_SUSPEND) | sig_bit(SIG_RESUME) | sig_bit(SIG_VCPU_RELINQUISH) | sig_bit(SIG_VCPU_SUSPEND);
const sigset_t SIGSET_PRIV_SYS = sig_bit(SIG_VCPU_RELINQUISH) | sig_bit(SIG_VCPU_SUSPEND);


errno_t sigset_init(sigset_t* _Nonnull set, int signo)
{
    *set = 0;
    return sigset_add(set, signo);
}

void sigset_clear(sigset_t* _Nonnull set)
{
    *set = 0;
}

void sigset_fill(sigset_t* _Nonnull set)
{
#if SIGSET_SIZE == UINT_WIDTH
    *set = UINT_MAX;
#else
#error "unknown sigset size"
#endif
}

errno_t sigset_add(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    *set |= sig_bit(signo);
    return EOK;
}

void sigset_addall(sigset_t* _Nonnull set, const sigset_t* _Nonnull oth)
{
    *set |= *oth;
}

errno_t sigset_remove(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        return EINVAL;
    }

    *set &= ~sig_bit(signo);
    return EOK;
}

void sigset_removeall(sigset_t* _Nonnull set, const sigset_t* _Nonnull oth)
{
    *set &= ~(*oth);
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

bool sigset_isempty(const sigset_t* _Nonnull set)
{
    return (*set == 0) ? true : false;
}
