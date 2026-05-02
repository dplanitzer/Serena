//
//  sigset.c
//  libc
//
//  Created by Dietmar Planitzer on 7/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <serena/sigset.h>


int sigset_init(sigset_t* _Nonnull set, int signo)
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

int sigset_add(sigset_t* _Nonnull set, int signo)
{
    if (signo < SIG_MIN || signo > SIG_MAX) {
        errno = EINVAL;
        return -1;
    }

    *set |= sig_bit(signo);
    return 0;
}

void sigset_addall(sigset_t* _Nonnull set, const sigset_t* _Nonnull oth)
{
    *set |= *oth;
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

void sigset_removeall(sigset_t* _Nonnull set, const sigset_t* _Nonnull oth)
{
    *set &= ~(*oth);
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

int sigset_isempty(const sigset_t* _Nonnull set)
{
    return (*set == 0) ? true : false;
}
